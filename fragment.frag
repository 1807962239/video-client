//三个分量的纹理采样器
uniform sampler2D uni_textureY;
uniform sampler2D uni_textureU;
uniform sampler2D uni_textureV;

varying vec2 out_uv;

void main(void)
{
    vec3 yuv;
    vec3 rgb;

    //根据纹理单元和纹理坐标获取每个分量的纹理信息
    //因为这里yuv分别都是单通道，单通道数据实际存储在红色通道
    //y亮度值是0到1
    yuv.x = texture2D(uni_textureY, out_uv).r;
    //uv分别是红色和蓝色的色差，范围是-0.5到0.5，而其获取到的原始范围是0到1，所以需要减0.5
    yuv.y = texture2D(uni_textureU, out_uv).r - 0.5;
    yuv.z = texture2D(uni_textureV, out_uv).r - 0.5;

    /* yuv转rgb的公式：
    *   rgb = mat3(
    *    1,       1,       1,        // 第一列
    *    0,       -0.39465, 2.03211, // 第二列
    *    1.13983, -0.58060, 0        // 第三列
    *    ) * yuv;
      */
    rgb = mat3( 1,1,1, 0,-0.39465,2.03211,1.13983,-0.58060,0) * yuv;
    //添加一个透明分量，得到一个vec4的最终颜色
    gl_FragColor = vec4(rgb, 1);
}
