// [config]
// expect_result: fail
// glsl_version: 1.20
// require_extensions: GL_ARB_uniform_buffer_object
// [end config]

#version 120
#extension GL_ARB_uniform_buffer_object: disable

uniform ub {
	vec4 a;
};

void foo(void) {
}
