// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#include <vespa/eval/eval/operation.h>
#include <vespa/eval/eval/simple_value.h>
#include <vespa/eval/eval/tensor_function.h>
#include <vespa/eval/eval/value_codec.h>
#include <vespa/eval/eval/value_type.h>
#include <vespa/eval/eval/node_types.h>
#include <vespa/vespalib/gtest/gtest.h>
#include <vespa/vespalib/util/stash.h>
#include <vespa/vespalib/util/stringfmt.h>
#include <map>

using namespace vespalib;
using namespace vespalib::eval;
using namespace vespalib::eval::tensor_function;

const auto &simple_factory = SimpleValueBuilderFactory::get();

struct EvalCtx {
    const ValueBuilderFactory &factory;
    Stash stash;
    std::vector<Value::UP> tensors;
    std::vector<Value::CREF> params;
    InterpretedFunction::UP ifun;
    std::unique_ptr<InterpretedFunction::Context> ictx;
    EvalCtx(const ValueBuilderFactory &factory_in)
        : factory(factory_in), stash(), tensors(), params(), ifun(), ictx() {}
    ~EvalCtx() {}
    size_t add_tensor(Value::UP tensor) {
        size_t id = params.size();
        params.emplace_back(*tensor);
        tensors.push_back(std::move(tensor));
        return id;
    }
    ValueType type_of(size_t idx) {
        return params[idx].get().type();
    }
    void replace_tensor(size_t idx, Value::UP tensor) {
        params[idx] = *tensor;
        tensors[idx] = std::move(tensor);
    }
    const Value &eval(const TensorFunction &fun) {
        ifun = std::make_unique<InterpretedFunction>(factory, fun);
        ictx = std::make_unique<InterpretedFunction::Context>(*ifun);
        return ifun->eval(*ictx, SimpleObjectParams(params));
    }
    Value::UP from_expr(const std::string &expr) {
        return value_from_spec(TensorSpec::from_expr(expr), factory);
    }
    Value::UP make_double(double value) {
        return value_from_spec(TensorSpec("double").add({}, value), factory);
    }
    Value::UP make_true() {
        return value_from_spec(TensorSpec("double").add({}, 1.0), factory);
    }
    Value::UP make_false() {
        return value_from_spec(TensorSpec("double").add({}, 0.0), factory);
    }
    Value::UP make_vector(std::initializer_list<double> cells, std::string dim = "x", bool mapped = false) {
        std::string type_spec = mapped
                                     ? make_string("tensor(%s{})", dim.c_str())
                                     : make_string("tensor(%s[%zu])", dim.c_str(), cells.size());
        TensorSpec spec(type_spec);
        size_t idx = 0;
        for (double cell_value: cells) {
            TensorSpec::Label label = mapped
                                      ? TensorSpec::Label(make_string("%zu", idx++))
                                      : TensorSpec::Label(idx++);
            spec.add({{dim, label}}, cell_value);
        }
        return value_from_spec(spec, factory);
    }
    Value::UP make_mixed_tensor(double a, double b, double c, double d) {
        return value_from_spec(
                TensorSpec("tensor(x{},y[2])")
                .add({{"x", "foo"}, {"y", 0}}, a)
                .add({{"x", "foo"}, {"y", 1}}, b)
                .add({{"x", "bar"}, {"y", 0}}, c)
                .add({{"x", "bar"}, {"y", 1}}, d), factory);
    }
    Value::UP make_tensor_matrix_first_half() {
        return value_from_spec(
                TensorSpec("tensor(x[2])")
                .add({{"x", 0}}, 1.0)
                .add({{"x", 1}}, 3.0), factory);
    }
    Value::UP make_tensor_matrix_second_half() {
        return value_from_spec(
                TensorSpec("tensor(x[2])")
                .add({{"x", 0}}, 2.0)
                .add({{"x", 1}}, 4.0), factory);
    }
    Value::UP make_tensor_matrix() {
        return value_from_spec(
                TensorSpec("tensor(x[2],y[2])")
                .add({{"x", 0}, {"y", 0}}, 1.0)
                .add({{"x", 0}, {"y", 1}}, 2.0)
                .add({{"x", 1}, {"y", 0}}, 3.0)
                .add({{"x", 1}, {"y", 1}}, 4.0), factory);
    }
    Value::UP make_float_tensor_matrix() {
        return value_from_spec(
                TensorSpec("tensor<float>(x[2],y[2])")
                .add({{"x", 0}, {"y", 0}}, 1.0)
                .add({{"x", 0}, {"y", 1}}, 2.0)
                .add({{"x", 1}, {"y", 0}}, 3.0)
                .add({{"x", 1}, {"y", 1}}, 4.0), factory);
    }
    Value::UP make_tensor_matrix_renamed() {
        return value_from_spec(
                TensorSpec("tensor(y[2],z[2])")
                .add({{"z", 0}, {"y", 0}}, 1.0)
                .add({{"z", 0}, {"y", 1}}, 2.0)
                .add({{"z", 1}, {"y", 0}}, 3.0)
                .add({{"z", 1}, {"y", 1}}, 4.0), factory);
    }
    Value::UP make_tensor_reduce_input() {
        return value_from_spec(
                TensorSpec("tensor(x[3],y[2])")
                .add({{"x",0},{"y",0}}, 1)
                .add({{"x",1},{"y",0}}, 2)
                .add({{"x",2},{"y",0}}, 3)
                .add({{"x",0},{"y",1}}, 4)
                .add({{"x",1},{"y",1}}, 5)
                .add({{"x",2},{"y",1}}, 6), factory);
    }
    Value::UP make_tensor_reduce_y_output() {
        return value_from_spec(
                TensorSpec("tensor(x[3])")
                .add({{"x",0}}, 5)
                .add({{"x",1}}, 7)
                .add({{"x",2}}, 9), factory);
    }
    Value::UP make_tensor_map_input() {
        return value_from_spec(
                TensorSpec("tensor(x{},y{})")
                .add({{"x","1"},{"y","1"}}, 1)
                .add({{"x","2"},{"y","1"}}, -3)
                .add({{"x","1"},{"y","2"}}, 5), factory);
    }
    Value::UP make_tensor_map_output() {
        return value_from_spec(
                TensorSpec("tensor(x{},y{})")
                .add({{"x","1"},{"y","1"}}, -1)
                .add({{"x","2"},{"y","1"}}, 3)
                .add({{"x","1"},{"y","2"}}, -5), factory);
    }
    Value::UP make_tensor_join_lhs() {
        return value_from_spec(
                TensorSpec("tensor(x{},y{})")
                .add({{"x","1"},{"y","1"}}, 1)
                .add({{"x","2"},{"y","1"}}, 3)
                .add({{"x","1"},{"y","2"}}, 5), factory);
    }
    Value::UP make_tensor_join_rhs() {
        return value_from_spec(
                TensorSpec("tensor(y{},z{})")
                .add({{"y","1"},{"z","1"}}, 7)
                .add({{"y","2"},{"z","1"}}, 11)
                .add({{"y","1"},{"z","2"}}, 13), factory);
    }
    Value::UP make_tensor_join_output() {
        return value_from_spec(
                TensorSpec("tensor(x{},y{},z{})")
                .add({{"x","1"},{"y","1"},{"z","1"}}, 7)
                .add({{"x","1"},{"y","1"},{"z","2"}}, 13)
                .add({{"x","2"},{"y","1"},{"z","1"}}, 21)
                .add({{"x","2"},{"y","1"},{"z","2"}}, 39)
                .add({{"x","1"},{"y","2"},{"z","1"}}, 55), factory);
    }
    Value::UP make_tensor_merge_lhs() {
        return value_from_spec(
                TensorSpec("tensor(x{})")
                .add({{"x","1"}}, 1)
                .add({{"x","2"}}, 3)
                .add({{"x","3"}}, 5), factory);
    }
    Value::UP make_tensor_merge_rhs() {
        return value_from_spec(
                TensorSpec("tensor(x{})")
                .add({{"x","2"}}, 7)
                .add({{"x","3"}}, 9)
                .add({{"x","4"}}, 11), factory);
    }
    Value::UP make_tensor_merge_output() {
        return value_from_spec(
                TensorSpec("tensor(x{})")
                .add({{"x","1"}}, 1)
                .add({{"x","2"}}, 10)
                .add({{"x","3"}}, 14)
                .add({{"x","4"}}, 11), factory);
    }
};

void verify_equal(const Value &expect, const Value &value) {
    auto expect_spec = spec_from_value(expect);
    auto value_spec = spec_from_value(value);
    EXPECT_EQ(expect_spec, value_spec);
}

TEST(TensorFunctionTest, require_that_const_value_works)
{
    EvalCtx ctx(simple_factory);
    Value::UP my_const = ctx.make_tensor_matrix();
    Value::UP expect = ctx.make_tensor_matrix();
    const auto &fun = const_value(*my_const, ctx.stash);
    EXPECT_TRUE(!fun.result_is_mutable());
    EXPECT_EQ(expect->type(), fun.result_type());
    verify_equal(*expect, ctx.eval(fun));
}

TEST(TensorFunctionTest, require_that_tensor_injection_works)
{
    EvalCtx ctx(simple_factory);
    size_t a_id = ctx.add_tensor(ctx.make_tensor_matrix());
    Value::UP expect = ctx.make_tensor_matrix();
    const auto &fun = inject(ValueType::from_spec("tensor(x[2],y[2])"), a_id, ctx.stash);
    EXPECT_TRUE(!fun.result_is_mutable());
    EXPECT_EQ(expect->type(), fun.result_type());
    verify_equal(*expect, ctx.eval(fun));
}

TEST(TensorFunctionTest, require_that_partial_tensor_reduction_works)
{
    EvalCtx ctx(simple_factory);
    size_t a_id = ctx.add_tensor(ctx.make_tensor_reduce_input());
    Value::UP expect = ctx.make_tensor_reduce_y_output();
    const auto &fun = reduce(inject(ValueType::from_spec("tensor(x[3],y[2])"), a_id, ctx.stash), Aggr::SUM, {"y"}, ctx.stash);
    EXPECT_TRUE(fun.result_is_mutable());
    EXPECT_EQ(expect->type(), fun.result_type());
    verify_equal(*expect, ctx.eval(fun));
}

TEST(TensorFunctionTest, require_that_full_tensor_reduction_works)
{
    EvalCtx ctx(simple_factory);
    size_t a_id = ctx.add_tensor(ctx.make_tensor_reduce_input());
    const auto &fun = reduce(inject(ValueType::from_spec("tensor(x[3],y[2])"), a_id, ctx.stash), Aggr::SUM, {}, ctx.stash);
    EXPECT_TRUE(fun.result_is_mutable());
    EXPECT_EQ(ValueType::double_type(), fun.result_type());
    const Value &result = ctx.eval(fun);
    EXPECT_TRUE(result.type().is_double());
    EXPECT_EQ(21.0, result.as_double());
}

TEST(TensorFunctionTest, require_that_tensor_map_works)
{
    EvalCtx ctx(simple_factory);
    size_t a_id = ctx.add_tensor(ctx.make_tensor_map_input());
    Value::UP expect = ctx.make_tensor_map_output();
    const auto &fun = map(inject(ValueType::from_spec("tensor(x{},y{})"), a_id, ctx.stash), operation::Neg::f, ctx.stash);
    EXPECT_TRUE(fun.result_is_mutable());
    EXPECT_EQ(expect->type(), fun.result_type());
    verify_equal(*expect, ctx.eval(fun));
}

TEST(TensorFunctionTest, require_that_tensor_join_works)
{
    EvalCtx ctx(simple_factory);
    size_t a_id = ctx.add_tensor(ctx.make_tensor_join_lhs());
    size_t b_id = ctx.add_tensor(ctx.make_tensor_join_rhs());
    Value::UP expect = ctx.make_tensor_join_output();
    const auto &fun = join(inject(ValueType::from_spec("tensor(x{},y{})"), a_id, ctx.stash),
                           inject(ValueType::from_spec("tensor(y{},z{})"), b_id, ctx.stash),
                           operation::Mul::f, ctx.stash);
    EXPECT_TRUE(fun.result_is_mutable());
    EXPECT_EQ(expect->type(), fun.result_type());
    verify_equal(*expect, ctx.eval(fun));
}

TEST(TensorFunctionTest, require_that_tensor_merge_works)
{
    EvalCtx ctx(simple_factory);
    size_t a_id = ctx.add_tensor(ctx.make_tensor_merge_lhs());
    size_t b_id = ctx.add_tensor(ctx.make_tensor_merge_rhs());
    Value::UP expect = ctx.make_tensor_merge_output();
    const auto &fun = merge(inject(ValueType::from_spec("tensor(x{})"), a_id, ctx.stash),
                            inject(ValueType::from_spec("tensor(x{})"), b_id, ctx.stash),
                            operation::Add::f, ctx.stash);
    EXPECT_TRUE(fun.result_is_mutable());
    EXPECT_EQ(expect->type(), fun.result_type());
    verify_equal(*expect, ctx.eval(fun));
}

TEST(TensorFunctionTest, require_that_tensor_concat_works)
{
    EvalCtx ctx(simple_factory);
    size_t a_id = ctx.add_tensor(ctx.make_tensor_matrix_first_half());
    size_t b_id = ctx.add_tensor(ctx.make_tensor_matrix_second_half());
    Value::UP expect = ctx.make_tensor_matrix();
    const auto &fun = concat(inject(ValueType::from_spec("tensor(x[2])"), a_id, ctx.stash),
                             inject(ValueType::from_spec("tensor(x[2])"), b_id, ctx.stash),
                             "y", ctx.stash);
    EXPECT_TRUE(fun.result_is_mutable());
    EXPECT_EQ(expect->type(), fun.result_type());
    verify_equal(*expect, ctx.eval(fun));
}

TEST(TensorFunctionTest, require_that_tensor_cell_cast_works)
{
    EvalCtx ctx(simple_factory);
    size_t a_id = ctx.add_tensor(ctx.make_tensor_matrix());
    Value::UP expect = ctx.make_float_tensor_matrix();
    const auto &fun = cell_cast(inject(ctx.type_of(a_id), a_id, ctx.stash), CellType::FLOAT, ctx.stash);
    EXPECT_TRUE(fun.result_is_mutable());
    EXPECT_EQ(expect->type(), fun.result_type());
    verify_equal(*expect, ctx.eval(fun));
}

TEST(TensorFunctionTest, require_that_cell_order_works) {
    EvalCtx ctx(simple_factory);
    size_t a_id = ctx.add_tensor(ctx.from_expr("tensor(x[3]):[5,7,3]"));
    Value::UP expect = ctx.from_expr("tensor(x[3]):[1,0,2]");
    const auto &a = inject(ctx.type_of(a_id), a_id, ctx.stash);
    const auto &op = cell_order(a, eval::CellOrder::MAX, ctx.stash);
    EXPECT_TRUE(op.result_is_mutable());
    EXPECT_EQ(expect->type(), op.result_type());
    verify_equal(*expect, ctx.eval(op));
    std::vector<TensorFunction::Child::CREF> refs;
    op.push_children(refs);
    ASSERT_EQ(refs.size(), 1u);
    EXPECT_EQ(&refs[0].get().get(), &a);
    fprintf(stderr, "%s", op.as_string().c_str());
}

TEST(TensorFunctionTest, require_that_map_subspaces_works) {
    EvalCtx ctx(simple_factory);
    size_t a_id = ctx.add_tensor(ctx.from_expr("tensor(x{}):{a:1,b:2,c:3}"));
    Value::UP expect = ctx.from_expr("tensor(x{}):{a:2,b:3,c:4}");
    const auto &a = inject(ctx.type_of(a_id), a_id, ctx.stash);
    auto inner_fun = Function::parse({"s"}, "s+1");
    const auto &op = map_subspaces(a, *inner_fun, NodeTypes(*inner_fun, {DoubleValue::shared_type()}), ctx.stash);
    EXPECT_TRUE(op.result_is_mutable());
    EXPECT_EQ(expect->type(), op.result_type());
    verify_equal(*expect, ctx.eval(op));
    std::vector<TensorFunction::Child::CREF> refs;
    op.push_children(refs);
    ASSERT_EQ(refs.size(), 1u);
    EXPECT_EQ(&refs[0].get().get(), &a);
    fprintf(stderr, "%s", op.as_string().c_str());
}

TEST(TensorFunctionTest, require_that_filter_subspaces_works) {
    EvalCtx ctx(simple_factory);
    size_t a_id = ctx.add_tensor(ctx.from_expr("tensor(x{}):{a:0,b:1,c:0}"));
    Value::UP expect = ctx.from_expr("tensor(x{}):{b:1}");
    const auto &a = inject(ctx.type_of(a_id), a_id, ctx.stash);
    auto inner_fun = Function::parse({"s"}, "s");
    const auto &op = filter_subspaces(a, *inner_fun, NodeTypes(*inner_fun, {DoubleValue::shared_type()}), ctx.stash);
    EXPECT_TRUE(op.result_is_mutable());
    EXPECT_EQ(expect->type(), op.result_type());
    verify_equal(*expect, ctx.eval(op));
    std::vector<TensorFunction::Child::CREF> refs;
    op.push_children(refs);
    ASSERT_EQ(refs.size(), 1u);
    EXPECT_EQ(&refs[0].get().get(), &a);
    fprintf(stderr, "%s", op.as_string().c_str());
}

TEST(TensorFunctionTest, require_that_tensor_create_works)
{
    EvalCtx ctx(simple_factory);
    size_t a_id = ctx.add_tensor(ctx.make_double(1.0));
    size_t b_id = ctx.add_tensor(ctx.make_double(2.0));
    Value::UP my_const = ctx.make_double(3.0);
    Value::UP expect = ctx.make_vector({1.0, 2.0, 3.0});
    const auto &a = inject(ValueType::double_type(), a_id, ctx.stash);
    const auto &b = inject(ValueType::double_type(), b_id, ctx.stash);
    const auto &c = const_value(*my_const, ctx.stash);
    const auto &fun = create(ValueType::from_spec("tensor(x[3])"),
                             {
                                 {{{"x", 0}}, a},
                                 {{{"x", 1}}, b},
                                 {{{"x", 2}}, c}
                             },
                             ctx.stash);
    EXPECT_TRUE(fun.result_is_mutable());
    EXPECT_EQ(expect->type(), fun.result_type());
    verify_equal(*expect, ctx.eval(fun));
}

TEST(TensorFunctionTest, require_that_single_value_tensor_peek_works)
{
    EvalCtx ctx(simple_factory);
    size_t a_id = ctx.add_tensor(ctx.make_double(1.0));
    size_t b_id = ctx.add_tensor(ctx.make_double(1000.0));
    Value::UP my_const = ctx.make_mixed_tensor(1.0, 2.0, 3.0, 4.0);
    Value::UP expect = ctx.make_vector({2.0, 3.0, 0.0});
    const auto &a = inject(ValueType::double_type(), a_id, ctx.stash);
    const auto &b = inject(ValueType::double_type(), b_id, ctx.stash);
    const auto &t = const_value(*my_const, ctx.stash);
    const auto &peek1 = peek(t, {{"x", "foo"}, {"y", a}}, ctx.stash);
    const auto &peek2 = peek(t, {{"x", "bar"}, {"y", size_t(0)}}, ctx.stash);
    const auto &peek3 = peek(t, {{"x", "bar"}, {"y", b}}, ctx.stash);
    const auto &fun = create(ValueType::from_spec("tensor(x[3])"),
                             {
                                 {{{"x", 0}}, peek1},
                                 {{{"x", 1}}, peek2},
                                 {{{"x", 2}}, peek3}
                             },
                             ctx.stash);
    EXPECT_TRUE(fun.result_is_mutable());
    EXPECT_EQ(expect->type(), fun.result_type());
    verify_equal(*expect, ctx.eval(fun));
}

TEST(TensorFunctionTest, require_that_tensor_subspace_tensor_peek_works)
{
    EvalCtx ctx(simple_factory);
    Value::UP my_const = ctx.make_mixed_tensor(1.0, 2.0, 3.0, 4.0);
    Value::UP expect = ctx.make_vector({3.0, 4.0}, "y");
    const auto &t = const_value(*my_const, ctx.stash);
    const auto &fun = peek(t, {{"x", "bar"}}, ctx.stash);
    EXPECT_TRUE(fun.result_is_mutable());
    EXPECT_EQ(expect->type(), fun.result_type());
    verify_equal(*expect, ctx.eval(fun));
}

TEST(TensorFunctionTest, require_that_automatic_string_conversion_tensor_peek_works)
{
    EvalCtx ctx(simple_factory);
    size_t a_id = ctx.add_tensor(ctx.make_double(1.0));
    Value::UP my_const = ctx.make_vector({1.0, 2.0, 3.0}, "x", true);
    const auto &a = inject(ValueType::double_type(), a_id, ctx.stash);
    const auto &t = const_value(*my_const, ctx.stash);
    const auto &fun = peek(t, {{"x", a}}, ctx.stash);
    EXPECT_TRUE(fun.result_is_mutable());
    EXPECT_TRUE(fun.result_type().is_double());
    const Value &result = ctx.eval(fun);
    EXPECT_TRUE(result.type().is_double());
    EXPECT_EQ(2.0, result.as_double());
}

TEST(TensorFunctionTest, require_that_tensor_rename_works)
{
    EvalCtx ctx(simple_factory);
    size_t a_id = ctx.add_tensor(ctx.make_tensor_matrix());
    Value::UP expect = ctx.make_tensor_matrix_renamed();
    const auto &fun = rename(inject(ValueType::from_spec("tensor(x[2],y[2])"), a_id, ctx.stash),
                             {"x"}, {"z"}, ctx.stash);
    EXPECT_TRUE(fun.result_is_mutable());
    EXPECT_EQ(expect->type(), fun.result_type());
    verify_equal(*expect, ctx.eval(fun));
}

TEST(TensorFunctionTest, require_that_if_node_works)
{
    EvalCtx ctx(simple_factory);
    size_t a_id = ctx.add_tensor(ctx.make_true());
    size_t b_id = ctx.add_tensor(ctx.make_tensor_matrix_first_half());
    size_t c_id = ctx.add_tensor(ctx.make_tensor_matrix_second_half());
    Value::UP expect_true = ctx.make_tensor_matrix_first_half();
    Value::UP expect_false = ctx.make_tensor_matrix_second_half();
    const auto &fun = if_node(inject(ValueType::double_type(), a_id, ctx.stash),
                              inject(ValueType::from_spec("tensor(x[2])"), b_id, ctx.stash),
                              inject(ValueType::from_spec("tensor(x[2])"), c_id, ctx.stash), ctx.stash);
    EXPECT_TRUE(!fun.result_is_mutable());
    EXPECT_EQ(expect_true->type(), fun.result_type());
    {
        SCOPED_TRACE("expect true");
        verify_equal(*expect_true, ctx.eval(fun));
    }
    ctx.replace_tensor(a_id, ctx.make_false());
    {
        SCOPED_TRACE("expect false");
        verify_equal(*expect_false, ctx.eval(fun));
    }
}

TEST(TensorFunctionTest, require_that_if_node_result_is_mutable_only_when_both_children_produce_mutable_results)
{
    Stash stash;
    const TensorFunction &cond = inject(DoubleValue::shared_type(), 0, stash);
    const TensorFunction &a = inject(ValueType::from_spec("tensor(x[2])"), 0, stash);
    const TensorFunction &b = inject(ValueType::from_spec("tensor(x[3])"), 0, stash);
    const TensorFunction &c = inject(ValueType::from_spec("tensor(x[5])"), 0, stash);
    const TensorFunction &tmp = concat(a, b, "x", stash); // will be mutable
    const TensorFunction &if_con_con = if_node(cond, c, c, stash);
    const TensorFunction &if_mut_con = if_node(cond, tmp, c, stash);
    const TensorFunction &if_con_mut = if_node(cond, c, tmp, stash);
    const TensorFunction &if_mut_mut = if_node(cond, tmp, tmp, stash);
    EXPECT_EQ(if_con_con.result_type(), c.result_type());
    EXPECT_EQ(if_con_mut.result_type(), c.result_type());
    EXPECT_EQ(if_mut_con.result_type(), c.result_type());
    EXPECT_EQ(if_mut_mut.result_type(), c.result_type());
    EXPECT_TRUE(!if_con_con.result_is_mutable());
    EXPECT_TRUE(!if_mut_con.result_is_mutable());
    EXPECT_TRUE(!if_con_mut.result_is_mutable());
    EXPECT_TRUE(if_mut_mut.result_is_mutable());
}

TEST(TensorFunctionTest, require_that_if_node_gets_expected_result_type)
{
    Stash stash;
    const TensorFunction &a = inject(DoubleValue::shared_type(), 0, stash);
    const TensorFunction &b = inject(ValueType::from_spec("tensor(x[2])"), 0, stash);
    const TensorFunction &c = inject(ValueType::from_spec("tensor(x[3])"), 0, stash);
    const TensorFunction &d = inject(ValueType::from_spec("error"), 0, stash);
    const TensorFunction &if_same = if_node(a, b, b, stash);
    const TensorFunction &if_different = if_node(a, b, c, stash);
    const TensorFunction &if_with_error = if_node(a, b, d, stash);
    EXPECT_EQ(if_same.result_type(), ValueType::from_spec("tensor(x[2])"));
    EXPECT_EQ(if_different.result_type(), ValueType::from_spec("error"));
    EXPECT_EQ(if_with_error.result_type(), ValueType::from_spec("error"));
}

TEST(TensorFunctionTest, require_that_push_children_works)
{
    Stash stash;
    std::vector<TensorFunction::Child::CREF> refs;
    const TensorFunction &a = inject(DoubleValue::shared_type(), 0, stash);
    const TensorFunction &b = inject(DoubleValue::shared_type(), 1, stash);
    const TensorFunction &c = const_value(stash.create<DoubleValue>(1.0), stash);
    a.push_children(refs);
    b.push_children(refs);
    c.push_children(refs);
    ASSERT_EQ(refs.size(), 0u);
    //-------------------------------------------------------------------------
    reduce(a, Aggr::SUM, {}, stash).push_children(refs);
    ASSERT_EQ(refs.size(), 1u);
    EXPECT_EQ(&refs[0].get().get(), &a);
    //-------------------------------------------------------------------------
    map(b, operation::Neg::f, stash).push_children(refs);
    ASSERT_EQ(refs.size(), 2u);
    EXPECT_EQ(&refs[1].get().get(), &b);
    //-------------------------------------------------------------------------
    join(a, b, operation::Add::f, stash).push_children(refs);
    ASSERT_EQ(refs.size(), 4u);
    EXPECT_EQ(&refs[2].get().get(), &a);
    EXPECT_EQ(&refs[3].get().get(), &b);
    //-------------------------------------------------------------------------
    merge(a, b, operation::Add::f, stash).push_children(refs);
    ASSERT_EQ(refs.size(), 6u);
    EXPECT_EQ(&refs[4].get().get(), &a);
    EXPECT_EQ(&refs[5].get().get(), &b);
    //-------------------------------------------------------------------------
    concat(a, b, "x", stash).push_children(refs);
    ASSERT_EQ(refs.size(), 8u);
    EXPECT_EQ(&refs[6].get().get(), &a);
    EXPECT_EQ(&refs[7].get().get(), &b);
    //-------------------------------------------------------------------------
    rename(c, {}, {}, stash).push_children(refs);
    ASSERT_EQ(refs.size(), 9u);
    EXPECT_EQ(&refs[8].get().get(), &c);
    //-------------------------------------------------------------------------
    if_node(a, b, c, stash).push_children(refs);
    ASSERT_EQ(refs.size(), 12u);
    EXPECT_EQ(&refs[9].get().get(), &a);
    EXPECT_EQ(&refs[10].get().get(), &b);
    EXPECT_EQ(&refs[11].get().get(), &c);
    //-------------------------------------------------------------------------
    cell_cast(a, CellType::FLOAT, stash).push_children(refs);
    ASSERT_EQ(refs.size(), 13u);
    EXPECT_EQ(&refs[12].get().get(), &a);
    //-------------------------------------------------------------------------
}

TEST(TensorFunctionTest, require_that_tensor_function_can_be_dumped_for_debugging)
{
    Stash stash;
    auto my_value_1 = stash.create<DoubleValue>(1.0);
    auto my_value_2 = stash.create<DoubleValue>(2.0);
    auto my_value_3 = stash.create<DoubleValue>(3.0);
    //-------------------------------------------------------------------------
    const auto &x5 = inject(ValueType::from_spec("tensor(x[5])"), 0, stash);
    const auto &float_x5 = cell_cast(x5, CellType::FLOAT, stash);
    const auto &double_x5 = cell_cast(float_x5, CellType::DOUBLE, stash);
    const auto &mapped_x5 = map(double_x5, operation::Relu::f, stash);
    const auto &const_1 = const_value(my_value_1, stash);
    const auto &joined_x5 = join(mapped_x5, const_1, operation::Mul::f, stash);
    //-------------------------------------------------------------------------
    const auto &peek1 = peek(x5, {{"x", const_1}}, stash);
    const auto &peek2 = peek(x5, {{"x", size_t(2)}}, stash);
    const auto &x2 = create(ValueType::from_spec("tensor(x[2])"),
                            {
                                {{{"x", 0}}, peek1},
                                {{{"x", 1}}, peek2}
                            }, stash);
    const auto &a3y10 = inject(ValueType::from_spec("tensor(a[3],y[10])"), 2, stash);
    const auto &a3 = reduce(a3y10, Aggr::SUM, {"y"}, stash);
    const auto &x3 = rename(a3, {"a"}, {"x"}, stash);
    const auto &concat_x5 = concat(x3, x2, "x", stash);
    //-------------------------------------------------------------------------
    const auto &const_2 = const_value(my_value_2, stash);
    const auto &const_3 = const_value(my_value_3, stash);
    const auto &merged_double = merge(const_2, const_3, operation::Less::f, stash);
    const auto &root = if_node(merged_double, joined_x5, concat_x5, stash);
    EXPECT_EQ(root.result_type(), ValueType::from_spec("tensor(x[5])"));
    fprintf(stderr, "function dump -->[[%s]]<-- function dump\n", root.as_string().c_str());
}

TEST(TensorFunctionTest, require_that_full_tensor_reduce_expands_dimension_list)
{
    Stash stash;
    const auto &num = inject(ValueType::from_spec("double"), 0, stash);
    const auto &mat = inject(ValueType::from_spec("tensor(x[5],y[5])"), 1, stash);
    const auto *reduce_num = as<Reduce>(reduce(num, Aggr::SUM, {}, stash));
    const auto *reduce_mat = as<Reduce>(reduce(mat, Aggr::SUM, {}, stash));
    ASSERT_TRUE(reduce_num);
    ASSERT_TRUE(reduce_mat);
    EXPECT_EQ(reduce_num->dimensions().size(), 0u);
    ASSERT_EQ(reduce_mat->dimensions().size(), 2u);
    EXPECT_EQ(reduce_mat->dimensions()[0], "x");
    EXPECT_EQ(reduce_mat->dimensions()[1], "y");
}

GTEST_MAIN_RUN_ALL_TESTS()
