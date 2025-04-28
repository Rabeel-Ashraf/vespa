// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#pragma once

#include "element_gap.h"
#include "fieldtype.h"
#include "filter_threshold.h"
#include <vespa/searchcommon/common/datatype.h>
#include <cstdint>
#include <string>

namespace search::fef {

const uint32_t IllegalFieldId = 0xffffffff;

/**
 * Information about a single field. This class is used by the @ref
 * IIndexEnvironment to expose information.
 **/
class FieldInfo
{
public:
    using CollectionType = search::index::schema::CollectionType;
    using DataType = search::index::schema::DataType;
    using string = std::string;
private:
    FieldType       _type;
    DataType        _data_type;
    CollectionType  _collection;
    string          _name;
    uint32_t        _id;
    FilterThreshold _threshold;
    ElementGap      _element_gap;
    bool            _hasAttribute;

public:
    /**
     * Create a new field info object. The id of a field acts as both
     * an index used to iterate all fields through the index
     * environment and as an enumeration of fields. Multiple fields
     * owned by the same index environment may not have the same name.
     **/
    FieldInfo(FieldType type_in, CollectionType collection_in,
              const string &name_in, uint32_t id_in);

    /**
     * Check if an attribute vector is available for this
     * field. Attributes are, and therefore have attributes. Index
     * fields may also have attributes available, or attributes may be
     * generated on-the-fly when needed. This function will tell you
     * whether attribute value lookup for a field will be possible.
     *
     *@return true if an attribute can be obtained for this field
     **/
    bool hasAttribute() const { return _hasAttribute; }

    /**
     * Add the power of attribute lookup to this field. This is used
     * to verify rank features using attributes during setup. If you
     * call this function to allow rank setup, but do not supply the
     * needed attributes during query execution; the poo is on you.
     **/
    void addAttribute() { _hasAttribute = true; }

    /**
     * Obtain the type of this field
     *
     * @return the type of this field
     **/
    FieldType type() const { return _type; }

    void set_data_type(DataType data_type_in) { _data_type = data_type_in; }
    DataType get_data_type() const { return _data_type; }

    /**
     * Obtain the collection type of this field
     *
     * @return collection type of this field
     **/
    CollectionType collection() const { return _collection; }

    /**
     * Obtain the name of this field
     *
     * @return the name of this field
     **/
    const string & name() const { return _name; }

    /**
     * Obtain the id of this field
     *
     * @return the id of this field
     **/
    uint32_t id() const { return _id; }

    /**
     * Set the flag indicating whether this field should be treated as
     * a filter field (fast searching and low complexity ranking).
     *
     * @param flag true if this field should be treated as a filter
     **/
    void setFilter(bool flag) { _threshold = FilterThreshold(flag); }

    /**
     * Obtain the flag indicating whether this field should be treated
     * as a filter field (fast searching and low complexity ranking).
     *
     * @return true if this field should be treated as a filter
     **/
    bool isFilter() const { return _threshold.is_filter(); }

    void set_filter_threshold(FilterThreshold threshold) noexcept { _threshold = threshold; }
    FilterThreshold get_filter_threshold() const noexcept { return _threshold; }
    void set_element_gap(ElementGap element_gap) noexcept { _element_gap = element_gap; }
    ElementGap get_element_gap() const noexcept { return _element_gap; }
};

}
