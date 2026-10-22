// RmlUi's blur vertex shader (RmlUi_SDL_GPU/shader_vert_blur.vert), written by hand: it hands the
// fragment stage an array of seven texture coordinates, which WGSL can't pass between stages, so
// they travel as seven separate values.

struct UniformBlockBlur {
    QuadPosition: vec4<f32>,
    QuadTexCoord: vec4<f32>,
    TexelOffset: vec2<f32>,
}

struct BlurVertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) texCoord0: vec2<f32>,
    @location(1) texCoord1: vec2<f32>,
    @location(2) texCoord2: vec2<f32>,
    @location(3) texCoord3: vec2<f32>,
    @location(4) texCoord4: vec2<f32>,
    @location(5) texCoord5: vec2<f32>,
    @location(6) texCoord6: vec2<f32>,
}

@group(1) @binding(0) var<uniform> blur: UniformBlockBlur;

const BLUR_NUM_WEIGHTS: i32 = 4;

fn offsetCoordinate(texCoord: vec2<f32>, i: i32) -> vec2<f32> {
    return texCoord - f32(i - BLUR_NUM_WEIGHTS + 1) * blur.TexelOffset;
}

@vertex
fn main(@location(2) inTexCoord: vec2<f32>) -> BlurVertexOutput {
    let texCoord = blur.QuadTexCoord.xy + inTexCoord * blur.QuadTexCoord.zw;
    var result: BlurVertexOutput;
    result.texCoord0 = offsetCoordinate(texCoord, 0);
    result.texCoord1 = offsetCoordinate(texCoord, 1);
    result.texCoord2 = offsetCoordinate(texCoord, 2);
    result.texCoord3 = offsetCoordinate(texCoord, 3);
    result.texCoord4 = offsetCoordinate(texCoord, 4);
    result.texCoord5 = offsetCoordinate(texCoord, 5);
    result.texCoord6 = offsetCoordinate(texCoord, 6);
    result.position = vec4<f32>(mix(blur.QuadPosition.xy, blur.QuadPosition.zw, inTexCoord), 0.0, 1.0);
    return result;
}
