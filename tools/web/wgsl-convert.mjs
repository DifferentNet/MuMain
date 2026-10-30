// SPIR-V to WGSL for the web build's WebGPU backend (libsdl-org/SDL#16020), shared by
// spirv-to-wgsl.mjs (the game's shaders) and rmlui-shaders-to-wgsl.mjs (RmlUi's).
//
// naga (wgpu's shader translator, npm package naga-wasm) does the translation. Three adjustments:
//
// 1. Bindings. SDL's SPIR-V and the backend's WGSL number the texture/storage groups (0: vertex,
//    2: fragment) differently:
//      SPIR-V: a sampled texture and its sampler share binding i; storage resources follow at numSamplers + k.
//      WGSL:   texture at 2i, its sampler at 2i + 1; storage resources follow at 2 * numSamplers + k.
//    naga rejects two resources on one binding, so the SPIR-V's Binding decorations are rewritten
//    before parsing. Uniform buffers (groups 1 and 3) keep theirs.
// 2. Clip space. naga's SPIR-V reader flips Y in vertex outputs (Vulkan to WebGPU convention), but
//    SDL already gives every backend the same clip space, so the flip it adds is taken out again.
// 3. Storage buffers. naga wraps a storage buffer's array in a struct of its own
//    (struct boneRows { u0040_data: array<vec4<f32>>, } and var<storage, read> boneRows_1: boneRows),
//    which Safari doesn't compile: its translation to Metal fails on that struct ("no matching
//    constructor" for it, Safari 27.0.1 on an M4 Mac, 2026-10-29), and every character, monster and
//    NPC is drawn with such a buffer (the skinned shaders' bones). The buffer is declared as the array
//    itself instead, which means the same to WebGPU (unwrapStorageArrays).
//
// The backend reads the bindings back out of the WGSL text with a simple parser: it must not see
// @group or var in comments, and it only treats a storage buffer as read-only if it says "read".

import { join } from "node:path";
import { pathToFileURL } from "node:url";

const OpTypeImage = 25, OpTypeSampler = 26, OpTypeSampledImage = 27, OpTypePointer = 32;
const OpVariable = 59, OpDecorate = 71;
const DecorationBinding = 33, DecorationDescriptorSet = 34;

export async function loadNaga(nagaDir) {
  return import(pathToFileURL(join(nagaDir, "dist", "node.js")).href);
}

// Returns a copy of the SPIR-V words with the texture/storage bindings renumbered (see above).
export function remapBindings(input) {
  const words = new Uint32Array(input);
  const typeOpcode = new Map(), pointee = new Map(), variableType = new Map();
  const set = new Map(), bindingWord = new Map();
  for (let i = 5; i < words.length;) {
    const count = words[i] >>> 16, opcode = words[i] & 0xffff;
    if (count === 0) throw new Error("malformed SPIR-V");
    if (opcode === OpTypeImage || opcode === OpTypeSampler || opcode === OpTypeSampledImage) typeOpcode.set(words[i + 1], opcode);
    else if (opcode === OpTypePointer) pointee.set(words[i + 1], words[i + 3]);
    else if (opcode === OpVariable) variableType.set(words[i + 2], words[i + 1]);
    else if (opcode === OpDecorate && words[i + 2] === DecorationDescriptorSet) set.set(words[i + 1], words[i + 3]);
    else if (opcode === OpDecorate && words[i + 2] === DecorationBinding) bindingWord.set(words[i + 1], i + 3);
    i += count;
  }

  const byGroup = new Map();
  for (const [id, group] of set) {
    if ((group !== 0 && group !== 2) || !bindingWord.has(id)) continue;
    const kind = typeOpcode.get(pointee.get(variableType.get(id)));
    const entry = { id, binding: words[bindingWord.get(id)], kind };
    (byGroup.get(group) ?? byGroup.set(group, []).get(group)).push(entry);
  }
  for (const entries of byGroup.values()) {
    const numSamplers = new Set(entries.filter((e) => e.kind !== undefined).map((e) => e.binding)).size;
    for (const e of entries) {
      let binding;
      if (e.kind === OpTypeSampler) binding = 2 * e.binding + 1;
      else if (e.kind !== undefined) binding = 2 * e.binding;  // image (or a combined image-sampler)
      else binding = e.binding + numSamplers;                  // storage buffer or storage texture
      words[bindingWord.get(e.id)] = binding;
    }
  }
  return words;
}

// naga's reader appends "let _eN = <position>.y; <position>.y = -(_eN);" to vertex entry points.
const yFlip = /^[ \t]*let (_e\d+) = ([\w.]+)\.y;\r?\n[ \t]*\2\.y = -\(\1\);\r?\n/m;

// A struct with one runtime-sized array member, used only as the type of storage buffers: those
// buffers become the array itself (adjustment 3 above), "buffer.member" becomes "buffer", and the
// struct goes.
const arrayStruct = /^struct (\w+) \{\n {4}(\w+): (array<[^\n]+>),\n\}\n\n?/gm;

function isRuntimeSized(arrayType) {
  // array<T> has no length; array<T, N> has one after a comma outside T's own angle brackets.
  let depth = 0;
  for (const c of arrayType.slice("array<".length, -1)) {
    if (c === "<") depth++;
    else if (c === ">") depth--;
    else if (c === "," && depth === 0) return false;
  }
  return true;
}

export function unwrapStorageArrays(wgsl) {
  const text = wgsl.replaceAll("\r\n", "\n");
  let out = text;
  for (const [definition, struct, member, arrayType] of text.matchAll(arrayStruct)) {
    if (!isRuntimeSized(arrayType)) continue;
    const declaration = new RegExp(String.raw`(var<storage[^>]*> (\w+)): ${struct};`, "g");
    const buffers = [...out.matchAll(declaration)].map((match) => match[2]);
    // The struct named anywhere else (a function's parameter, another struct): leave it.
    const uses = out.match(new RegExp(String.raw`\b${struct}\b`, "g")).length;
    if (buffers.length === 0 || uses !== 1 + buffers.length) continue;
    out = out.replace(definition, "").replace(declaration, `$1: ${arrayType};`);
    for (const buffer of buffers) out = out.replaceAll(`${buffer}.${member}`, buffer);
  }
  return out;
}

// Converts SPIR-V (bytes or words) to WGSL; name only labels errors.
export function convert(naga, spirv, name) {
  const words = spirv instanceof Uint32Array ? spirv
    : new Uint32Array(spirv.buffer.slice(spirv.byteOffset, spirv.byteOffset + spirv.byteLength));
  const module = naga.parseSpirv(remapBindings(words));
  let wgsl = naga.writeWgsl(module, naga.validate(module));
  if (/@vertex/.test(wgsl)) {
    wgsl = wgsl.replace(yFlip, "");
    if (/\.y = -\(/.test(wgsl)) throw new Error(`${name}: unexpected Y flip left in the WGSL`);
  }
  return unwrapStorageArrays(wgsl.replaceAll("var<storage>", "var<storage, read>"));
}
