// RmlUi's blur fragment shader (RmlUi_SDL_GPU/shader_frag_blur.frag), written by hand to match
// shader_vert_blur.wgsl: the seven texture coordinates arrive as seven separate values.

struct UniformBlockBlur {
    Weights: vec4<f32>,
    TexCoordMin: vec2<f32>,
    TexCoordMax: vec2<f32>,
}

@group(2) @binding(0) var blurTexture: texture_2d<f32>;
@group(2) @binding(1) var blurSampler: sampler;
@group(3) @binding(0) var<uniform> blur: UniformBlockBlur;

const BLUR_NUM_WEIGHTS: i32 = 4;

fn tap(texCoord: vec2<f32>, i: i32) -> vec4<f32> {
    let inRegion = step(blur.TexCoordMin, texCoord) * step(texCoord, blur.TexCoordMax);
    return textureSample(blurTexture, blurSampler, texCoord) * inRegion.x * inRegion.y
        * blur.Weights[abs(i - BLUR_NUM_WEIGHTS + 1)];
}

@fragment
fn main(@location(0) t0: vec2<f32>, @location(1) t1: vec2<f32>, @location(2) t2: vec2<f32>,
        @location(3) t3: vec2<f32>, @location(4) t4: vec2<f32>, @location(5) t5: vec2<f32>,
        @location(6) t6: vec2<f32>) -> @location(0) vec4<f32> {
    return tap(t0, 0) + tap(t1, 1) + tap(t2, 2) + tap(t3, 3) + tap(t4, 4) + tap(t5, 5) + tap(t6, 6);
}
