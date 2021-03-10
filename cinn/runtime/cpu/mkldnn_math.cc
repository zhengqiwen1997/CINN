#include "cinn/runtime/cpu/mkldnn_math.h"

#include <vector>

#include "cinn/backends/extern_func_jit_register.h"
#include "cinn/common/cas.h"

using mkldnn::algorithm;
using mkldnn::memory;
using tag = memory::format_tag;
using dt  = memory::data_type;

void cinn_cpu_mkldnn_conv2d_nchw_fp32(int batch_size,
                                      int c_in,
                                      int input_h,
                                      int input_w,
                                      int c_out,
                                      int group,
                                      int filter_h,
                                      int filter_w,
                                      int pad_h,
                                      int pad_w,
                                      int stride_h,
                                      int stride_w,
                                      int dilation_h,
                                      int dilation_w,
                                      cinn_buffer_t* inputs,
                                      cinn_buffer_t* weights,
                                      cinn_buffer_t* out) {
  auto cpu_engine = mkldnn::engine(mkldnn::engine::kind::cpu, 0);
  mkldnn::stream cpu_stream(cpu_engine);

  memory::dims conv_src_tz     = {batch_size, c_in, input_h, input_w};
  memory::dims conv_weights_tz = {c_out, c_in, filter_h, filter_w};
  if (group > 1) {
    conv_weights_tz = {group, c_out / group, c_in / group, filter_h, filter_w};
  }
  int out_h                   = (input_h - ((filter_h - 1) * dilation_h + 1) + 2 * pad_h) / stride_h + 1;
  int out_w                   = (input_w - ((filter_w - 1) * dilation_w + 1) + 2 * pad_w) / stride_w + 1;
  memory::dims conv_dst_tz    = {batch_size, c_out, out_h, out_w};
  memory::dims conv_strides   = {stride_h, stride_w};
  memory::dims conv_paddings  = {pad_h, pad_w};
  memory::dims conv_dilations = {dilation_h - 1, dilation_w - 1};

  auto conv_user_src_memory =
      memory({{conv_src_tz}, dt::f32, tag::nchw}, cpu_engine, reinterpret_cast<float*>(inputs->memory));
  auto conv_user_weights_memory = memory({{conv_weights_tz}, dt::f32, group > 1 ? tag::goihw : tag::oihw},
                                         cpu_engine,
                                         reinterpret_cast<float*>(weights->memory));
  auto conv_user_dst_memory =
      memory({{conv_dst_tz}, dt::f32, tag::nchw}, cpu_engine, reinterpret_cast<float*>(out->memory));

  auto conv_src_md     = memory::desc({conv_src_tz}, dt::f32, tag::any);
  auto conv_weights_md = memory::desc({conv_weights_tz}, dt::f32, tag::any);
  auto conv_dst_md     = memory::desc({conv_dst_tz}, dt::f32, tag::nchw);

  auto conv_desc = mkldnn::convolution_forward::desc(mkldnn::prop_kind::forward_inference,
                                                     mkldnn::algorithm::convolution_direct,
                                                     conv_src_md,
                                                     conv_weights_md,
                                                     conv_dst_md,
                                                     conv_strides,
                                                     conv_dilations,
                                                     conv_paddings,
                                                     conv_paddings);

  auto conv_prim_desc = mkldnn::convolution_forward::primitive_desc(conv_desc, cpu_engine);

  auto conv_src_memory     = conv_user_src_memory;
  auto conv_weights_memory = conv_user_weights_memory;
  auto conv_dst_memory     = conv_user_dst_memory;
  if (conv_prim_desc.dst_desc() != conv_user_dst_memory.get_desc()) {
    conv_dst_memory = memory(conv_prim_desc.dst_desc(), cpu_engine);
  }
  auto conv = mkldnn::convolution_forward(conv_prim_desc);
  conv.execute(cpu_stream,
               {{MKLDNN_ARG_SRC, conv_src_memory},
                {MKLDNN_ARG_WEIGHTS, conv_weights_memory},
                {MKLDNN_ARG_DST, conv_dst_memory}});
  if (conv_prim_desc.dst_desc() != conv_user_dst_memory.get_desc()) {
    mkldnn::reorder(conv_dst_memory, conv_user_dst_memory).execute(cpu_stream, conv_dst_memory, conv_user_dst_memory);
  } else {
    conv_user_dst_memory = conv_dst_memory;
  }

  cpu_stream.wait();
}

CINN_REGISTER_HELPER(cinn_cpu_mkldnn) {
  using namespace cinn;  // NOLINT
  using backends::FunctionProto;
  auto host_target = common::DefaultHostTarget();

  FunctionProto::shape_inference_t inference_shape_conv2d_nchw = [](const std::vector<Expr>& args, int offset) {
    CHECK_EQ(args.size(), 16UL) << "Wrong number of arguments passed in";
    auto N         = common::AutoSimplify(args[0]);
    int input_h    = common::AutoSimplify(args[2]).as_int32();
    int input_w    = common::AutoSimplify(args[3]).as_int32();
    auto c_out     = common::AutoSimplify(args[4]);
    int filter_h   = common::AutoSimplify(args[6]).as_int32();
    int filter_w   = common::AutoSimplify(args[7]).as_int32();
    int pad_h      = common::AutoSimplify(args[8]).as_int32();
    int pad_w      = common::AutoSimplify(args[9]).as_int32();
    int stride_h   = common::AutoSimplify(args[10]).as_int32();
    int stride_w   = common::AutoSimplify(args[11]).as_int32();
    int dilation_h = common::AutoSimplify(args[12]).as_int32();
    int dilation_w = common::AutoSimplify(args[13]).as_int32();
    int out_h      = (input_h - ((filter_h - 1) * dilation_h + 1) + 2 * pad_h) / stride_h + 1;
    int out_w      = (input_w - ((filter_w - 1) * dilation_w + 1) + 2 * pad_w) / stride_w + 1;

    std::vector<Expr> shape;
    shape.push_back(N);
    shape.push_back(c_out);
    shape.push_back(Expr(out_h));
    shape.push_back(Expr(out_w));
    return shape;
  };

  REGISTER_EXTERN_FUNC_HELPER(cinn_cpu_mkldnn_conv2d_nchw_fp32, host_target)
      .SetRetType<void>()
      .AddInputType<int>()              // batch_size
      .AddInputType<int>()              // c_in
      .AddInputType<int>()              // input_h
      .AddInputType<int>()              // input_w
      .AddInputType<int>()              // c_out
      .AddInputType<int>()              // group
      .AddInputType<int>()              // filter_h
      .AddInputType<int>()              // filter_w
      .AddInputType<int>()              // pad_h
      .AddInputType<int>()              // pad_w
      .AddInputType<int>()              // stride_h
      .AddInputType<int>()              // stride_w
      .AddInputType<int>()              // dilation_h
      .AddInputType<int>()              // dilation_w
      .AddInputType<cinn_buffer_t*>()   // inputs
      .AddInputType<cinn_buffer_t*>()   // weights
      .AddOutputType<cinn_buffer_t*>()  // out
      .SetShapeInference(inference_shape_conv2d_nchw)
      .End();

  return true;
}
