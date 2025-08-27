// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#include "mysearch.h"
#include <vespa/searchlib/queryeval/flow.h>
#include <vespa/searchlib/queryeval/blueprint.h>
#include <vespa/searchlib/queryeval/intermediate_blueprints.h>
#include <vespa/vespalib/objects/objectdumper.h>
#include <vespa/vespalib/objects/visit.h>
#include <vespa/vespalib/data/slime/slime.h>
#include <algorithm>
#include <vespa/vespalib/gtest/gtest.h>

#include <vespa/log/log.h>
LOG_SETUP("blueprint_test");

using namespace search::queryeval;
using MatchData = search::fef::MatchData;

namespace {

//-----------------------------------------------------------------------------

class MyOr : public IntermediateBlueprint
{
private:
    AnyFlow my_flow(InFlow in_flow) const override {
        return AnyFlow::create<OrFlow>(in_flow);
    }
public:
    ~MyOr() override;
    FlowStats calculate_flow_stats(uint32_t) const final {
        return {OrFlow::estimate_of(get_children()),
                OrFlow::cost_of(get_children(), false),
                OrFlow::cost_of(get_children(), true)};
    }
    HitEstimate combine(const std::vector<HitEstimate> &data) const override {
        return max(data);
    }

    FieldSpecBaseList exposeFields() const override {
        return mixChildrenFields();
    }

    void sort(Children &children, InFlow) const override {
        std::sort(children.begin(), children.end(), TieredGreaterEstimate());
    }

    SearchIterator::UP
    createIntermediateSearch(MultiSearch::Children subSearches, MatchData &md) const override {
        return std::make_unique<MySearch>("or", std::move(subSearches), &md, strict());
    }
    SearchIteratorUP createFilterSearchImpl(FilterConstraint constraint) const override {
        return create_default_filter(constraint);
    }
    static MyOr& create() { return *(new MyOr()); }
    MyOr& add(Blueprint *n) { addChild(UP(n)); return *this; }
    MyOr& add(Blueprint &n) { addChild(UP(&n)); return *this; }
};

MyOr::~MyOr() = default;

class OtherOr : public OrBlueprint
{
private:
public:
    SearchIterator::UP
    createIntermediateSearch(MultiSearch::Children subSearches, MatchData &md) const override {
        return std::make_unique<MySearch>("or", std::move(subSearches), &md, strict());
    }

    static OtherOr& create() { return *(new OtherOr()); }
    OtherOr& add(Blueprint *n) { addChild(UP(n)); return *this; }
    OtherOr& add(Blueprint &n) { addChild(UP(&n)); return *this; }
};

//-----------------------------------------------------------------------------

class MyAnd : public AndBlueprint
{
private:
public:
    HitEstimate combine(const std::vector<HitEstimate> &data) const override {
        return min(data);
    }

    FieldSpecBaseList exposeFields() const override {
        return {};
    }

    SearchIterator::UP
    createIntermediateSearch(MultiSearch::Children subSearches, MatchData &md) const override {
        return std::make_unique<MySearch>("and", std::move(subSearches), &md, strict());
    }

    static MyAnd& create() { return *(new MyAnd()); }
    MyAnd& add(Blueprint *n) { addChild(UP(n)); return *this; }
    MyAnd& add(Blueprint &n) { addChild(UP(&n)); return *this; }
};


class OtherAnd : public AndBlueprint
{
private:
public:
    SearchIterator::UP
    createIntermediateSearch(MultiSearch::Children subSearches, MatchData &md) const override {
        return std::make_unique<MySearch>("and", std::move(subSearches), &md, strict());
    }

    static OtherAnd& create() { return *(new OtherAnd()); }
    OtherAnd& add(Blueprint *n) { addChild(UP(n)); return *this; }
    OtherAnd& add(Blueprint &n) { addChild(UP(&n)); return *this; }
};

class OtherAndNot : public AndNotBlueprint
{
public:
    SearchIterator::UP
    createIntermediateSearch(MultiSearch::Children subSearches, MatchData &md) const override {
        return std::make_unique<MySearch>("andnot", std::move(subSearches), &md, strict());
    }

    static OtherAndNot& create() { return *(new OtherAndNot()); }
    OtherAndNot& add(Blueprint *n) { addChild(UP(n)); return *this; }
    OtherAndNot& add(Blueprint &n) { addChild(UP(&n)); return *this; }

};

//-----------------------------------------------------------------------------

struct MyTerm : SimpleLeafBlueprint {
    MyTerm(FieldSpecBase field, uint32_t hitEstimate) : SimpleLeafBlueprint(field) {
        setEstimate(HitEstimate(hitEstimate, false));
    }
    ~MyTerm() override;
    FlowStats calculate_flow_stats(uint32_t docid_limit) const override {
        return default_flow_stats(docid_limit, getState().estimate().estHits, 0);
    }
    SearchIterator::UP createLeafSearch(const search::fef::TermFieldMatchDataArray &) const override {
        return {};
    }
    SearchIteratorUP createFilterSearchImpl(FilterConstraint constraint) const override {
        return create_default_filter(constraint);
    }
};

MyTerm::~MyTerm() = default;

//-----------------------------------------------------------------------------

Blueprint::UP ap(Blueprint *b) { return Blueprint::UP(b); }
Blueprint::UP ap(Blueprint &b) { return Blueprint::UP(&b); }

} // namespace <unnamed>

class Fixture
{
private:
    MatchData::UP _md;
public:
    Fixture()
        : _md(MatchData::makeTestInstance(100, 10))
    {}
    ~Fixture() = default;
    SearchIterator::UP create(const Blueprint &blueprint);
    void check_equal(const Blueprint &a, const Blueprint &b, const std::string& label);
    void check_not_equal(const Blueprint &a, const Blueprint &b, const std::string& label);
    static void check_equal(const SearchIterator &a, const SearchIterator &b, const std::string& label);
    static void check_not_equal(const SearchIterator &a, const SearchIterator &b, const std::string& label);
};

SearchIterator::UP
Fixture::create(const Blueprint &blueprint)
{
    const_cast<Blueprint &>(blueprint).null_plan(true, 1000);
    const_cast<Blueprint &>(blueprint).fetchPostings(ExecuteInfo::FULL);
    SearchIterator::UP search = blueprint.createSearch(*_md);
    MySearch::verifyAndInfer(search.get(), *_md);
    return search;
}

void
Fixture::check_equal(const SearchIterator &a, const SearchIterator &b, const std::string& label)
{
    SCOPED_TRACE(label);
    EXPECT_EQ(a.asString(), b.asString());
}

void
Fixture::check_not_equal(const SearchIterator &a, const SearchIterator &b, const std::string& label)
{
    SCOPED_TRACE(label);
    EXPECT_NE(a.asString(), b.asString());
}

void
Fixture::check_equal(const Blueprint &a, const Blueprint &b, const std::string& label)
{
    SearchIterator::UP searchA = create(a);
    SearchIterator::UP searchB = create(b);
    check_equal(*searchA, *searchB, label);
}

void
Fixture::check_not_equal(const Blueprint &a, const Blueprint &b, const std::string& label)
{
    SearchIterator::UP searchA = create(a);
    SearchIterator::UP searchB = create(b);
    check_not_equal(*searchA, *searchB, label);
}

Blueprint::UP
buildBlueprint1()
{
    return ap(MyAnd::create()
              .add(MyOr::create()
                   .add(MyLeafSpec(10).addField(1, 11).create())
                   .add(MyLeafSpec(20).addField(1, 21).create())
                   .add(MyLeafSpec(30).addField(1, 31).create())
                   )
              .add(MyOr::create()
                   .add(MyLeafSpec(100).addField(2, 22).create())
                   .add(MyLeafSpec(200).addField(2, 42).create())
                   )
              );
}

Blueprint::UP
buildBlueprint2()
{
    return ap(MyAnd::create()
              .add(MyOr::create()
                   .add(MyLeafSpec(10).addField(1, 11).create())
                   .add(MyLeafSpec(20).addField(1, 21).create())
                   )
              .add(MyOr::create()
                   .add(MyLeafSpec(100).addField(2, 22).create())
                   .add(MyLeafSpec(200).addField(2, 32).create())
                   .add(MyLeafSpec(300).addField(2, 42).create())
                   )
              );
}

TEST(BlueprintTest, testBlueprintBuilding)
{
    Fixture f;
    Blueprint::UP root1 = buildBlueprint1();
    Blueprint::UP root2 = buildBlueprint2();
    SearchIterator::UP search1 = f.create(*root1);
    SearchIterator::UP search2 = f.create(*root2);
}

TEST(BlueprintTest, testHitEstimateCalculation)
{
    {
        Blueprint::UP leaf = ap(MyLeafSpec(37).create());
        EXPECT_EQ(37u, leaf->getState().estimate().estHits);
        EXPECT_EQ(0u, leaf->getState().numFields());
    }
    {
        Blueprint::UP a1 = ap(MyAnd::create()
                              .add(MyLeafSpec(7).addField(1, 11).create())
                              .add(MyLeafSpec(4).addField(1, 21).create())
                              .add(MyLeafSpec(6).addField(1, 31).create()));
        EXPECT_EQ(4u, a1->getState().estimate().estHits);
    }
    {
        Blueprint::UP a2 = ap(MyAnd::create()
                              .add(MyLeafSpec(4).addField(1, 1).create())
                              .add(MyLeafSpec(7).addField(2, 2).create())
                              .add(MyLeafSpec(6).addField(3, 3).create()));
        EXPECT_EQ(4u, a2->getState().estimate().estHits);
    }
    {
        Blueprint::UP o1 = ap(MyOr::create()
                              .add(MyLeafSpec(7).addField(1, 11).create())
                              .add(MyLeafSpec(4).addField(1, 21).create())
                              .add(MyLeafSpec(6).addField(1, 31).create()));
        EXPECT_EQ(7u, o1->getState().estimate().estHits);
    }
    {
        Blueprint::UP o2 = ap(MyOr::create()
                              .add(MyLeafSpec(4).addField(1, 1).create())
                              .add(MyLeafSpec(7).addField(2, 2).create())
                              .add(MyLeafSpec(6).addField(3, 3).create()));
        EXPECT_EQ(7u, o2->getState().estimate().estHits);
    }
    {
        Blueprint::UP a = ap(MyAnd::create()
                             .add(MyLeafSpec(0).create())
                             .add(MyLeafSpec(0, true).create()));
        EXPECT_EQ(0u, a->getState().estimate().estHits);
        EXPECT_EQ(true, a->getState().estimate().empty);
    }
    {
        Blueprint::UP o = ap(MyOr::create()
                             .add(MyLeafSpec(0).create())
                             .add(MyLeafSpec(0, true).create()));
        EXPECT_EQ(0u, o->getState().estimate().estHits);
        EXPECT_EQ(false, o->getState().estimate().empty);
    }
    {
        Blueprint::UP tree1 = buildBlueprint1();
        EXPECT_EQ(30u, tree1->getState().estimate().estHits);

        Blueprint::UP tree2 = buildBlueprint2();
        EXPECT_EQ(20u, tree2->getState().estimate().estHits);
    }
}

TEST(BlueprintTest, testHitEstimatePropagation)
{
    auto *leaf1 = new MyLeaf();
    leaf1->estimate(10);

    auto *leaf2 = new MyLeaf();
    leaf2->estimate(20);

    auto *leaf3 = new MyLeaf();
    leaf3->estimate(30);

    MyOr *parent = new MyOr();
    MyOr *grandparent = new MyOr();

    Blueprint::UP root(grandparent);

    parent->addChild(ap(leaf1));
    parent->addChild(ap(leaf3));
    grandparent->addChild(ap(leaf2));
    grandparent->addChild(ap(parent));
    EXPECT_EQ(30u, root->getState().estimate().estHits);

    // edit
    leaf3->estimate(50);
    EXPECT_EQ(50u, root->getState().estimate().estHits);

    // remove
    ASSERT_TRUE(parent->childCnt() == 2);
    Blueprint::UP tmp = parent->removeChild(1);
    ASSERT_TRUE(tmp.get() == leaf3);
    EXPECT_EQ(1u, parent->childCnt());
    EXPECT_EQ(20u, root->getState().estimate().estHits);

    // add
    leaf3->estimate(25);
    EXPECT_EQ(20u, root->getState().estimate().estHits);
    parent->addChild(std::move(tmp));
    EXPECT_FALSE(tmp);
    EXPECT_EQ(25u, root->getState().estimate().estHits);
}

TEST(BlueprintTest, testMatchDataPropagation)
{
    {
        Blueprint::UP leaf = ap(MyLeafSpec(0, true).create());
        EXPECT_EQ(0u, leaf->getState().numFields());
    }
    {
        Blueprint::UP leaf = ap(MyLeafSpec(42)
                                .addField(1, 41)
                                .addField(2, 72).create());
        EXPECT_EQ(42u, leaf->getState().estimate().estHits);
        ASSERT_TRUE(leaf->getState().numFields() == 2);
        EXPECT_EQ(1u, leaf->getState().field(0).getFieldId());
        EXPECT_EQ(2u, leaf->getState().field(1).getFieldId());
        EXPECT_EQ(41u, leaf->getState().field(0).getHandle());
        EXPECT_EQ(72u, leaf->getState().field(1).getHandle());
    }
    {
        Blueprint::UP a = ap(MyAnd::create()
                             .add(MyLeafSpec(7).addField(1, 11).create())
                             .add(MyLeafSpec(4).addField(1, 21).create())
                             .add(MyLeafSpec(6).addField(1, 31).create()));
        EXPECT_EQ(0u, a->getState().numFields());
    }
    {
        MyOr &o = MyOr::create()
                  .add(MyLeafSpec(1).addField(1, 1).create())
                  .add(MyLeafSpec(2).addField(2, 2).create());

        Blueprint::UP a = ap(o);
        ASSERT_TRUE(a->getState().numFields() == 2);
        EXPECT_EQ(1u, a->getState().field(0).getFieldId());
        EXPECT_EQ(2u, a->getState().field(1).getFieldId());
        EXPECT_EQ(1u, a->getState().field(0).getHandle());
        EXPECT_EQ(2u, a->getState().field(1).getHandle());
        EXPECT_EQ(2u, a->getState().estimate().estHits);

        o.add(MyLeafSpec(5).addField(2, 2).create());
        ASSERT_TRUE(a->getState().numFields() == 2);
        EXPECT_EQ(1u, a->getState().field(0).getFieldId());
        EXPECT_EQ(2u, a->getState().field(1).getFieldId());
        EXPECT_EQ(1u, a->getState().field(0).getHandle());
        EXPECT_EQ(2u, a->getState().field(1).getHandle());
        EXPECT_EQ(5u, a->getState().estimate().estHits);

        o.add(MyLeafSpec(5).addField(2, 32).create());
        EXPECT_EQ(0u, a->getState().numFields());
        o.removeChild(3);
        EXPECT_EQ(2u, a->getState().numFields());
        o.add(MyLeafSpec(0, true).create());
        EXPECT_EQ(0u, a->getState().numFields());
    }
}

TEST(BlueprintTest, testChildAndNotCollapsing)
{
    Fixture f;
    Blueprint::UP unsorted = ap(OtherAndNot::create()
                                .add(OtherAndNot::create()
                                     .add(OtherAndNot::create()
                                          .add(MyLeafSpec(200).addField(1, 11).create())
                                          .add(MyLeafSpec(100).addField(1, 21).create())
                                          .add(MyLeafSpec(300).addField(1, 31).create())
                                          )
                                     .add(OtherAnd::create()
                                          .add(MyLeafSpec(1).addField(2, 42).create())
                                          .add(MyLeafSpec(2).addField(2, 52).create())
                                          .add(MyLeafSpec(3).addField(2, 62).create())
                                          )
                                     )
                                .add(MyLeafSpec(30).addField(3, 73).create())
                                .add(MyLeafSpec(20).addField(3, 83).create())
                                .add(MyLeafSpec(10).addField(3, 93).create())
                                );

    Blueprint::UP sorted = ap(OtherAndNot::create()
                              .add(MyLeafSpec(200).addField(1, 11).create())
                              .add(MyLeafSpec(300).addField(1, 31).create())
                              .add(MyLeafSpec(100).addField(1, 21).create())
                              .add(MyLeafSpec(30).addField(3, 73).create())
                              .add(MyLeafSpec(20).addField(3, 83).create())
                              .add(MyLeafSpec(10).addField(3, 93).create())
                              .add(OtherAnd::create()
                                   .add(MyLeafSpec(1).addField(2, 42).create())
                                   .add(MyLeafSpec(2).addField(2, 52).create())
                                   .add(MyLeafSpec(3).addField(2, 62).create())
                                   )
                              );
    f.check_not_equal(*sorted, *unsorted, "before optimize and sort");
    unsorted->setDocIdLimit(1000);
    unsorted = Blueprint::optimize_and_sort(std::move(unsorted));
    f.check_equal(*sorted, *unsorted, "after optimize and sort");
}

TEST(BlueprintTest, testChildAndCollapsing)
{
    Fixture f;
    Blueprint::UP unsorted = ap(OtherAnd::create()
                                .add(OtherAnd::create()
                                     .add(OtherAnd::create()
                                          .add(MyLeafSpec(200).addField(1, 11).create())
                                          .add(MyLeafSpec(100).addField(1, 21).create())
                                          .add(MyLeafSpec(300).addField(1, 31).create())
                                          )
                                     .add(OtherAnd::create()
                                          .add(MyLeafSpec(1).addField(2, 42).create())
                                          .add(MyLeafSpec(2).addField(2, 52).create())
                                          .add(MyLeafSpec(3).addField(2, 62).create())
                                          )
                                     )
                                .add(OtherAnd::create()
                                     .add(MyLeafSpec(30).addField(3, 73).create())
                                     .add(MyLeafSpec(20).addField(3, 83).create())
                                     .add(MyLeafSpec(10).addField(3, 93).create())
                                     )
                                );

    Blueprint::UP sorted = ap(OtherAnd::create()
                                   .add(MyLeafSpec(1).addField(2, 42).create())
                                   .add(MyLeafSpec(2).addField(2, 52).create())
                                   .add(MyLeafSpec(3).addField(2, 62).create())
                                   .add(MyLeafSpec(10).addField(3, 93).create())
                                   .add(MyLeafSpec(20).addField(3, 83).create())
                                   .add(MyLeafSpec(30).addField(3, 73).create())
                                   .add(MyLeafSpec(100).addField(1, 21).create())
                                   .add(MyLeafSpec(200).addField(1, 11).create())
                                   .add(MyLeafSpec(300).addField(1, 31).create())
                              );

    f.check_not_equal(*sorted, *unsorted, "before optimize and sort");
    unsorted->setDocIdLimit(1000);
    unsorted = Blueprint::optimize_and_sort(std::move(unsorted));
    f.check_equal(*sorted, *unsorted, "after optimize and sort");
}

TEST(BlueprintTest, testChildOrCollapsing)
{
    Fixture f;
    Blueprint::UP unsorted = ap(OtherOr::create()
                                .add(OtherOr::create()
                                     .add(OtherOr::create()
                                          .add(MyLeafSpec(200).addField(1, 11).create())
                                          .add(MyLeafSpec(100).addField(1, 21).create())
                                          .add(MyLeafSpec(300).addField(1, 31).create())
                                          )
                                     .add(OtherOr::create()
                                          .add(MyLeafSpec(1).addField(2, 42).create())
                                          .add(MyLeafSpec(2).addField(2, 52).create())
                                          .add(MyLeafSpec(3).addField(2, 62).create())
                                          )
                                     )
                                .add(OtherOr::create()
                                     .add(MyLeafSpec(30).addField(3, 73).create())
                                     .add(MyLeafSpec(20).addField(3, 83).create())
                                     .add(MyLeafSpec(10).addField(3, 93).create())
                                     )
                                );

    Blueprint::UP sorted = ap(OtherOr::create()
                                   .add(MyLeafSpec(300).addField(1, 31).create())
                                   .add(MyLeafSpec(200).addField(1, 11).create())
                                   .add(MyLeafSpec(100).addField(1, 21).create())
                                   .add(MyLeafSpec(30).addField(3, 73).create())
                                   .add(MyLeafSpec(20).addField(3, 83).create())
                                   .add(MyLeafSpec(10).addField(3, 93).create())
                                   .add(MyLeafSpec(3).addField(2, 62).create())
                                   .add(MyLeafSpec(2).addField(2, 52).create())
                                   .add(MyLeafSpec(1).addField(2, 42).create())
                              );
    f.check_not_equal(*sorted, *unsorted, "before optimize and sort");
    unsorted->setDocIdLimit(1000);
    // we sort non-strict here since a strict OR does not have a
    // deterministic sort order.
    unsorted = Blueprint::optimize_and_sort(std::move(unsorted), false);
    f.check_equal(*sorted, *unsorted, "after optimize and sort");
}

TEST(BlueprintTest, testChildSorting)
{
    Fixture f;
    Blueprint::UP unsorted = ap(MyAnd::create()
                                .add(MyOr::create()
                                     .add(MyLeafSpec(200).addField(1, 11).create())
                                     .add(MyLeafSpec(100).addField(1, 21).create())
                                     .add(MyLeafSpec(300).addField(1, 31).create())
                                     )
                                .add(MyOr::create()
                                     .add(MyLeafSpec(1).addField(2, 42).create())
                                     .add(MyLeafSpec(2).addField(2, 52).create())
                                     .add(MyLeafSpec(3).addField(2, 62).create())
                                     )
                                .add(MyOr::create()
                                     .add(MyLeafSpec(30).addField(3, 73).create())
                                     .add(MyLeafSpec(20).addField(3, 83).create())
                                     .add(MyLeafSpec(10).addField(3, 93).create())
                                     )
                                );

    Blueprint::UP sorted = ap(MyAnd::create()
                              .add(MyOr::create()
                                   .add(MyLeafSpec(3).addField(2, 62).create())
                                   .add(MyLeafSpec(2).addField(2, 52).create())
                                   .add(MyLeafSpec(1).addField(2, 42).create())
                                   )
                              .add(MyOr::create()
                                   .add(MyLeafSpec(30).addField(3, 73).create())
                                   .add(MyLeafSpec(20).addField(3, 83).create())
                                   .add(MyLeafSpec(10).addField(3, 93).create())
                                   )
                              .add(MyOr::create()
                                   .add(MyLeafSpec(300).addField(1, 31).create())
                                   .add(MyLeafSpec(200).addField(1, 11).create())
                                   .add(MyLeafSpec(100).addField(1, 21).create())
                                   )
                              );

    f.check_not_equal(*sorted, *unsorted, "before optimize and sort");
    unsorted->setDocIdLimit(1000);
    unsorted = Blueprint::optimize_and_sort(std::move(unsorted));
    f.check_equal(*sorted, *unsorted, "after optimize and sort");
}


TEST(BlueprintTest, testSearchCreation)
{
    Fixture f;
    {
        Blueprint::UP l = ap(MyLeafSpec(3)
                             .addField(1, 1)
                             .addField(2, 2)
                             .addField(3, 3).create());
        SearchIterator::UP leafsearch = f.create(*l);

        auto *lw = new MySearch("leaf", true, true);
        lw->addHandle(1).addHandle(2).addHandle(3);
        SearchIterator::UP wantleaf(lw);

        f.check_equal(*wantleaf, *leafsearch, "create leafsearch");
    }
    {
        Blueprint::UP a = ap(MyAnd::create()
                             .add(MyLeafSpec(1).addField(1, 1).create())
                             .add(MyLeafSpec(2).addField(2, 2).create()));
        SearchIterator::UP andsearch = f.create(*a);

        auto *l1 = new MySearch("leaf", true, true);
        auto *l2 = new MySearch("leaf", true, false);
        l1->addHandle(1);
        l2->addHandle(2);
        auto *aw = new MySearch("and", false, true);
        aw->add(l1);
        aw->add(l2);
        SearchIterator::UP wanted(aw);
        f.check_equal(*wanted, *andsearch, "create and search");
    }
    {
        Blueprint::UP o = ap(MyOr::create()
                             .add(MyLeafSpec(1).addField(1, 11).create())
                             .add(MyLeafSpec(2).addField(2, 22).create()));
        SearchIterator::UP orsearch = f.create(*o);

        auto *l1 = new MySearch("leaf", true, true);
        auto *l2 = new MySearch("leaf", true, true);
        l1->addHandle(11);
        l2->addHandle(22);
        auto *ow = new MySearch("or", false, true);
        ow->add(l1);
        ow->add(l2);
        SearchIterator::UP wanted(ow);
        f.check_equal(*wanted, *orsearch, "create or search");
    }
}

TEST(BlueprintTest, testBlueprintMakeNew)
{
    Blueprint::UP orig = ap(MyOr::create()
                            .add(MyLeafSpec(1).addField(1, 11).create())
                            .add(MyLeafSpec(2).addField(2, 22).create()));
    orig->setSourceId(42);
    MyOr *myOr = dynamic_cast<MyOr*>(orig.get());
    ASSERT_TRUE(myOr != nullptr);
    EXPECT_EQ(42u, orig->getSourceId());
    EXPECT_EQ(2u, orig->getState().numFields());
}

std::string
getExpectedBlueprint()
{
    return "(anonymous namespace)::MyOr {\n"
           "    isTermLike: true\n"
           "    fields: FieldList {\n"
           "        [0]: Field {\n"
           "            fieldId: 5\n"
           "            handle: 7\n"
           "            isFilter: false\n"
           "        }\n"
           "    }\n"
           "    estimate: HitEstimate {\n"
           "        empty: false\n"
           "        estHits: 9\n"
           "        cost_tier: 1\n"
           "        tree_size: 2\n"
           "        allow_termwise_eval: false\n"
           "    }\n"
           "    relative_estimate: 0\n"
           "    cost: 0\n"
           "    strict_cost: 0\n"
           "    sourceId: 4294967295\n"
           "    docid_limit: 0\n"
           "    id: 0\n"
           "    strict: false\n"
           "    children: std::vector {\n"
           "        [0]: (anonymous namespace)::MyTerm {\n"
           "            isTermLike: true\n"
           "            fields: FieldList {\n"
           "                [0]: Field {\n"
           "                    fieldId: 5\n"
           "                    handle: 7\n"
           "                    isFilter: false\n"
           "                }\n"
           "            }\n"
           "            estimate: HitEstimate {\n"
           "                empty: false\n"
           "                estHits: 9\n"
           "                cost_tier: 1\n"
           "                tree_size: 1\n"
           "                allow_termwise_eval: true\n"
           "            }\n"
           "            relative_estimate: 0\n"
           "            cost: 0\n"
           "            strict_cost: 0\n"
           "            sourceId: 4294967295\n"
           "            docid_limit: 0\n"
           "            id: 0\n"
           "            strict: false\n"
           "        }\n"
           "    }\n"
           "}\n";
}

std::string
getExpectedSlimeBlueprint() {
    return "{"
           "    '[type]': '(anonymous namespace)::MyOr',"
           "     isTermLike: true,"
           "     fields: {"
           "        '[type]': 'FieldList',"
           "        '[0]': {"
           "            '[type]': 'Field',"
           "            fieldId: 5,"
           "            handle: 7,"
           "            isFilter: false"
           "        }"
           "    },"
           "    estimate: {"
           "        '[type]': 'HitEstimate',"
           "        empty: false,"
           "        estHits: 9,"
           "        cost_tier: 1,"
           "        tree_size: 2,"
           "        allow_termwise_eval: false"
           "    },"
           "    relative_estimate: 0.0,"
           "    cost: 0.0,"
           "    strict_cost: 0.0,"
           "    sourceId: 4294967295,"
           "    docid_limit: 0,"
           "    id: 0,"
           "    strict: false,"
           "    children: {"
           "        '[type]': 'std::vector',"
           "        '[0]': {"
           "            isTermLike: true,"
           "            fields: {"
           "                '[type]': 'FieldList',"
           "                '[0]': {"
           "                    '[type]': 'Field',"
           "                    fieldId: 5,"
           "                    handle: 7,"
           "                    isFilter: false"
           "                }"
           "            },"
           "            '[type]': '(anonymous namespace)::MyTerm',"
           "            estimate: {"
           "                '[type]': 'HitEstimate',"
           "                empty: false,"
           "                estHits: 9,"
           "                cost_tier: 1,"
           "                tree_size: 1,"
           "                allow_termwise_eval: true"
           "            },"
           "            relative_estimate: 0.0,"
           "            cost: 0.0,"
           "            strict_cost: 0.0,"
           "            sourceId: 4294967295,"
           "            docid_limit: 0,"
           "            id: 0,"
           "            strict: false"
           "        }"
           "    }"
           "}";
}


struct BlueprintFixture
{
    MyOr _blueprint;
    BlueprintFixture() : _blueprint() {
        _blueprint.add(new MyTerm(FieldSpecBase(5, 7), 9));
    }
};

TEST(BlueprintTest, requireThatAsStringWorks)
{
    BlueprintFixture f;
    EXPECT_EQ(getExpectedBlueprint(), f._blueprint.asString());
}

TEST(BlueprintTest, requireThatAsSlimeWorks)
{
    BlueprintFixture f;
    vespalib::Slime slime;
    f._blueprint.asSlime(vespalib::slime::SlimeInserter(slime));
    auto s = slime.toString();
    vespalib::Slime expectedSlime;
    vespalib::slime::JsonFormat::decode(getExpectedSlimeBlueprint(), expectedSlime);
    EXPECT_EQ(expectedSlime, slime);
}

TEST(BlueprintTest, requireThatVisitMembersWorks)
{
    BlueprintFixture f;
    vespalib::ObjectDumper dumper;
    visit(dumper, "", &f._blueprint);
    EXPECT_EQ(getExpectedBlueprint(), dumper.toString());
}

TEST(BlueprintTest, requireThatDocIdLimitInjectionWorks)
{
    BlueprintFixture f;
    ASSERT_GT(f._blueprint.childCnt(), 0u);
    const MyTerm &term = dynamic_cast<MyTerm&>(f._blueprint.getChild(0));
    EXPECT_EQ(0u, term.get_docid_limit());
    f._blueprint.setDocIdLimit(1000);
    EXPECT_EQ(1000u, term.get_docid_limit());
}

TEST(BlueprintTest, Control_object_sizes) {
    EXPECT_EQ(32u, sizeof(Blueprint::State));
    EXPECT_EQ(56u, sizeof(Blueprint));
    EXPECT_EQ(88u, sizeof(LeafBlueprint));
}

Blueprint::Options make_opts(bool sort_by_cost, bool allow_force_strict, bool keep_order) {
    return Blueprint::Options().sort_by_cost(sort_by_cost).allow_force_strict(allow_force_strict).keep_order(keep_order);
}

void check_opts(bool sort_by_cost, bool allow_force_strict, bool keep_order) {
    EXPECT_EQ(Blueprint::opt_sort_by_cost(), sort_by_cost);
    EXPECT_EQ(Blueprint::opt_allow_force_strict(), allow_force_strict);
    EXPECT_EQ(Blueprint::opt_keep_order(), keep_order);
}

TEST(BlueprintTest, Options_binding_and_nesting) {
    check_opts(false, false, false);
    {
        auto opts_guard1 = Blueprint::bind_opts(make_opts(true, true, false));
        check_opts(true, true, false);
        {
            auto opts_guard2 = Blueprint::bind_opts(make_opts(false, false, true));
            check_opts(false, false, true);
        }
        check_opts(true, true, false);
    }
    check_opts(false, false, false);
}

TEST(BlueprintTest, self_strict_resolving_during_sort) {
    uint32_t docs = 1000;
    uint32_t hits = 250;
    auto leaf = std::unique_ptr<Blueprint>(MyLeafSpec(hits).create());
    leaf->setDocIdLimit(docs);
    leaf->update_flow_stats(docs);
    EXPECT_EQ(leaf->estimate(), 0.25);
    EXPECT_EQ(leaf->cost(), 1.0);
    EXPECT_EQ(leaf->strict_cost(), 0.25);
    EXPECT_EQ(leaf->strict(), false); // starting value
    { // do not allow force strict
        auto guard = Blueprint::bind_opts(make_opts(true, false, false));
        leaf->sort(true);
        EXPECT_EQ(leaf->strict(), true);
        leaf->sort(false);
        EXPECT_EQ(leaf->strict(), false);
    }
    { // allow force strict
        auto guard = Blueprint::bind_opts(make_opts(true, true, false));
        leaf->sort(true);
        EXPECT_EQ(leaf->strict(), true);
        leaf->sort(false);
        EXPECT_EQ(leaf->strict(), true);
        leaf->sort(0.30);
        EXPECT_EQ(leaf->strict(), true);
        leaf->sort(0.20);
        EXPECT_EQ(leaf->strict(), false);
    }
}

void check_ids(Blueprint &bp, const std::vector<uint32_t> &expect, const std::string& label) {
    SCOPED_TRACE(label);
    std::vector<uint32_t> actual;
    bp.each_node_post_order([&](auto &node){ actual.push_back(node.id()); });
    ASSERT_EQ(actual.size(), expect.size());
    for (size_t i = 0; i < actual.size(); ++i) {
        EXPECT_EQ(actual[i], expect[i]);
    }
}

TEST(BlueprintTest, blueprint_node_enumeration) {
    auto a = std::make_unique<AndBlueprint>();
    a->addChild(std::make_unique<MyLeaf>());
    a->addChild(std::make_unique<MyLeaf>());
    auto b = std::make_unique<AndBlueprint>();
    b->addChild(std::make_unique<MyLeaf>());
    b->addChild(std::make_unique<MyLeaf>());
    auto root = std::make_unique<OrBlueprint>();
    root->addChild(std::move(a));
    root->addChild(std::move(b));
    check_ids(*root, {0,0,0,0,0,0,0}, "before enumerate");
    root->enumerate(1);
    check_ids(*root, {3,4,2,6,7,5,1}, "after enumerate");
}

GTEST_MAIN_RUN_ALL_TESTS()
