// attribute限定符只存在于vertex shader中，可以在数据缓冲区中读取数据，全局只读
attribute vec4 attr_position; // 顶点坐标
attribute vec2 attr_uv; // uv坐标

// uniform全局只读，但在fragment也能使用
uniform mat4   uni_mat; // 传入的变换矩阵

// varying是vertex和fragment之间传递数据的，可以在vertex中改，在fragment中使用
varying vec2   out_uv; // 传给片段着色器的uv坐标

void main(void)
{
    out_uv = attr_uv;
    gl_Position = uni_mat * attr_position; // 顶点坐标左乘变换矩阵
}
