/*******************************************************************************
* Copyright 2021 Intel Corporation
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/

#include "oneapi/dal/table/backend/csr_kernels.hpp"
#include "oneapi/dal/table/backend/convert.hpp"
#include "oneapi/dal/backend/common.hpp"

namespace oneapi::dal::backend {

using error_msg = dal::detail::error_messages;

ONEDAL_FORCEINLINE void check_origin_data(const array<byte_t>& origin_data,
                                          std::int64_t element_count,
                                          std::int64_t origin_dtype_size,
                                          std::int64_t block_dtype_size) {
    detail::check_mul_overflow(element_count, std::max(origin_dtype_size, block_dtype_size));
    ONEDAL_ASSERT(origin_data.get_count() >= element_count * origin_dtype_size);
}

/// Provides access to the block of rows in `data` array of the table in CSR format.
/// The method returns an array that directly points to the memory within the table
/// if it is possible. In that case, the array refers to the memory as to immutable data.
/// Otherwise, the new memory block is allocated, the data from the table rows is converted
/// and copied into this block. In this case, the array refers to the block as to mutable data.
///
/// @tparam Policy       Execution policy type. The :literal:`Policy` type should be `default_host_policy`
///                      or `data_parallel_policy`.
/// @tparam BlockData    The type of elements in the output array. In case the data values in CSR table
///                      are stored in the different data type, the conversion to `BlockData` type needs
///                      to be performed.
///                      The :literal:`BlockData` type should be at least :expr:`float`, :expr:`double`
///                      or :expr:`std::int32_t`.
///
/// @param[in] policy               Execution policy.
/// @param[in] origin_info          Information about this block of rows in the table.
///                                 Contains: layout, number of rows, information about data type,
///                                 indexing, etc.
/// @param[in] origin_data          Memory block that stores `data` array in the CSR table.
/// @param[in] same_data_type       True if the data type of the data in CSR table is the same as the data
///                                 type requested by the accessor's pull method. False otherwise.
/// @param[in] origin_offset        Index of the starting element of the `data` array in CSR table
///                                 to be pulled.
/// @param[in] block_size           Number of elemenst of the `data` array in CSR table to be pulled.
/// @param[out] data                Array that stores the block of rows of `data` array pulled
///                                 from the CSR table.
/// @param[in] kind                 The requested kind of USM in the returned block.
/// @param[in] preserve_mutability  True if the mutability should be preserved in the output array.
///                                 False otherwise.
template <typename Policy, typename BlockData>
void pull_data_impl(const Policy& policy,
                    const csr_info& origin_info,
                    const array<byte_t>& origin_data,
                    const bool same_data_type,
                    const std::int64_t origin_offset,
                    const std::int64_t block_size,
                    array<BlockData>& data,
                    alloc_kind kind,
                    bool preserve_mutability) {
    constexpr std::int64_t block_dtype_size = sizeof(BlockData);
    constexpr data_type block_dtype = detail::make_data_type<BlockData>();

    const auto origin_dtype_size = detail::get_data_type_size(origin_info.dtype_);

    if (same_data_type && !alloc_kind_requires_copy(get_alloc_kind(origin_data), kind)) {
        refer_origin_data(origin_data,
                          origin_offset * block_dtype_size,
                          block_size,
                          data,
                          preserve_mutability);
    }
    else {
        if (data.get_count() < block_size || !data.has_mutable_data() ||
            alloc_kind_requires_copy(get_alloc_kind(data), kind)) {
            reset_array(policy, data, block_size, kind);
        }

        const byte_t* const src_data = origin_data.get_data() + origin_offset * origin_dtype_size;
        BlockData* const dst_data = data.get_mutable_data();

        backend::convert_vector(policy,
                                src_data + origin_offset * block_dtype_size,
                                dst_data,
                                origin_info.dtype_,
                                block_dtype,
                                block_size);
    }
}

/// Provides access to the block of rows in `column_indices` array of the table in CSR format.
/// The method returns an array that directly points to the memory within the table
/// if it is possible. In that case, the array refers to the memory as to immutable data.
/// Otherwise, the new memory block is allocated, the data from the table rows is converted
/// and copied into this block. In this case, the array refers to the block as to mutable data.
///
/// @tparam Policy       Execution policy type. The :literal:`Policy` type should be `default_host_policy`
///                      or `data_parallel_policy`.
///
/// @param[in] policy                   Execution policy.
/// @param[in] origin_column_indices    `column_indices` array in the CSR table.
/// @param[in] origin_offset            Index of the starting element of the `column_indices` array
///                                     in CSR table to be pulled.
/// @param[in] block_size               Number of elemenst of the `column_indices` array in CSR table
///                                     to be pulled.
/// @param[in] indices_offset           The offset between the indices in the CSR table and the indices
///                                     requested by the pull method:
///                                         0, if the indexing is the same in the table
///                                            and in the pull method;
///                                         1, if the table has zero-based indexing
///                                            and pull method resuests one=based indexing;
///                                        -1, if the table has one-based indexing
///                                            and pull method resuests zero=based indexing.
/// @param[out] column_indices      Array that stores the block of rows of `column_indices` array pulled
///                                 from the CSR table.
/// @param[in] kind                 The requested kind of USM in the returned block.
/// @param[in] preserve_mutability  True if the mutability should be preserved in the output array.
///                                 False otherwise.
template <typename Policy>
void pull_column_indices_impl(const Policy& policy,
                              const array<std::int64_t>& origin_column_indices,
                              const std::int64_t origin_offset,
                              const std::int64_t block_size,
                              const std::int64_t indices_offset,
                              array<std::int64_t>& column_indices,
                              alloc_kind kind,
                              bool preserve_mutability) {
    if (!alloc_kind_requires_copy(get_alloc_kind(origin_column_indices), kind) &&
        indices_offset == 0) {
        refer_origin_data(origin_column_indices,
                          origin_offset,
                          block_size,
                          column_indices,
                          preserve_mutability);
    }
    else {
        reset_array(policy, column_indices, block_size, kind);

        const auto dtype_size = sizeof(std::int64_t);
        const std::int64_t* const src_data =
            origin_column_indices.get_data() + origin_offset * dtype_size;
        std::int64_t* const dst_data = column_indices.get_mutable_data();

        backend::convert_vector(policy,
                                src_data + origin_offset * dtype_size,
                                dst_data,
                                data_type::int64,
                                data_type::int64,
                                block_size);

        if (indices_offset != 0) {
            shift_array_values(policy, dst_data, block_size, indices_offset);
        }
    }
}

/// Provides access to the block of values in `row_offsets` array of the table in CSR format.
/// The method returns an array that directly points to the memory within the table
/// if it is possible. In that case, the array refers to the memory as to immutable data.
/// Otherwise, the new memory block is allocated, the data from the table rows is converted
/// and copied into this block. In this case, the array refers to the block as to mutable data.
///
/// @tparam Policy       Execution policy type. The :literal:`Policy` type should be `default_host_policy`
///                      or `data_parallel_policy`.
///
/// @param[in] policy                   Execution policy.
/// @param[in] origin_row_offsets       `row_offsets` array in the CSR table.
/// @param[in] block_info               Information about the block of rows requested by the pull method.
///                                     Contains: layout, number of rows, information about data type,
///                                     indexing, etc.
/// @param[in] indices_offset           The offset between the indices in the CSR table and the indices
///                                     requested by the pull method:
///                                         0, if the indexing is the same in the table
///                                            and in the pull method;
///                                         1, if the table has zero-based indexing
///                                            and pull method resuests one=based indexing;
///                                        -1, if the table has one-based indexing
///                                            and pull method resuests zero=based indexing.
/// @param[out] row_offsets         Array that stores the block of values of `row_offsets` array pulled
///                                 from the CSR table.
/// @param[in] kind                 The requested kind of USM in the returned block.
/// @param[in] preserve_mutability  True if the mutability should be preserved in the output array.
///                                 False otherwise.
template <typename Policy>
void pull_row_offsets_impl(const Policy& policy,
                           const array<std::int64_t>& origin_row_offsets,
                           const block_info& block_info,
                           const std::int64_t indices_offset,
                           array<std::int64_t>& row_offsets,
                           alloc_kind kind,
                           bool preserve_mutability) {
    if (row_offsets.get_count() < block_info.row_count_ + 1 || !row_offsets.has_mutable_data() ||
        alloc_kind_requires_copy(get_alloc_kind(row_offsets), kind)) {
        reset_array(policy, row_offsets, block_info.row_count_ + 1, kind);
    }
    if (block_info.row_offset_ == 0 && indices_offset == 0) {
        refer_origin_data(origin_row_offsets,
                          0,
                          block_info.row_count_,
                          row_offsets,
                          preserve_mutability);
    }
    else {
        const std::int64_t* const src_row_offsets = origin_row_offsets.get_data();
        std::int64_t* const dst_row_offsets = row_offsets.get_mutable_data();
        const std::int64_t dst_row_offsets_count = block_info.row_count_ + 1;

        for (std::int64_t i = 0; i < dst_row_offsets_count; i++) {
            dst_row_offsets[i] = src_row_offsets[block_info.row_offset_ + i] -
                                 src_row_offsets[block_info.row_offset_] + 1;
        }

        if (indices_offset != 0) {
            shift_array_values(policy, dst_row_offsets, dst_row_offsets_count, indices_offset);
        }
    }
}

template <typename Policy, typename BlockData>
void pull_csr_block_impl(const Policy& policy,
                         const csr_info& origin_info,
                         const block_info& block_info,
                         const array<byte_t>& origin_data,
                         const array<std::int64_t>& origin_column_indices,
                         const array<std::int64_t>& origin_row_offsets,
                         array<BlockData>& data,
                         array<std::int64_t>& column_indices,
                         array<std::int64_t>& row_offsets,
                         alloc_kind kind,
                         bool preserve_mutability) {
    constexpr std::int64_t block_dtype_size = sizeof(BlockData);
    constexpr data_type block_dtype = detail::make_data_type<BlockData>();

    const auto origin_dtype_size = detail::get_data_type_size(origin_info.dtype_);

    // overflows checked here
    check_origin_data(origin_data, origin_info.element_count_, origin_dtype_size, block_dtype_size);

    const std::int64_t origin_offset =
        origin_row_offsets[block_info.row_offset_] - origin_row_offsets[0];
    ONEDAL_ASSERT(origin_offset >= 0);

    const std::int64_t block_size =
        origin_row_offsets[block_info.row_offset_ + block_info.row_count_] -
        origin_row_offsets[block_info.row_offset_];
    ONEDAL_ASSERT(block_size >= 0);

    const bool same_data_type(block_dtype == origin_info.dtype_);

    const int indices_offset = (origin_info.indexing_ == block_info.indexing_)
                                   ? 0
                                   : ((origin_info.indexing_ == sparse_indexing::zero_based &&
                                       block_info.indexing_ == sparse_indexing::one_based)
                                          ? 1
                                          : -1);
    pull_data_impl<Policy, BlockData>(policy,
                                      origin_info,
                                      origin_data,
                                      same_data_type,
                                      origin_offset,
                                      block_size,
                                      data,
                                      kind,
                                      preserve_mutability);

    pull_column_indices_impl<Policy>(policy,
                                     origin_column_indices,
                                     origin_offset,
                                     block_size,
                                     indices_offset,
                                     column_indices,
                                     kind,
                                     preserve_mutability);

    pull_row_offsets_impl<Policy>(policy,
                                  origin_row_offsets,
                                  block_info,
                                  indices_offset,
                                  row_offsets,
                                  kind,
                                  preserve_mutability);
}

template <typename Policy, typename BlockData>
void csr_pull_block(const Policy& policy,
                    const csr_info& origin_info,
                    const block_info& block_info,
                    const array<byte_t>& origin_data,
                    const array<std::int64_t>& origin_column_indices,
                    const array<std::int64_t>& origin_row_offsets,
                    array<BlockData>& data,
                    array<std::int64_t>& column_indices,
                    array<std::int64_t>& row_offsets,
                    alloc_kind requested_alloc_kind,
                    bool preserve_mutability) {
    switch (origin_info.layout_) {
        case data_layout::row_major:
            pull_csr_block_impl(policy,
                                origin_info,
                                block_info,
                                origin_data,
                                origin_column_indices,
                                origin_row_offsets,
                                data,
                                column_indices,
                                row_offsets,
                                requested_alloc_kind,
                                preserve_mutability);
            break;
        default: throw dal::domain_error(error_msg::unsupported_data_layout());
    }
}

#define INSTANTIATE(Policy, BlockData)                                             \
    template void csr_pull_block(const Policy& policy,                             \
                                 const csr_info& origin_info,                      \
                                 const block_info& block_info,                     \
                                 const array<byte_t>& origin_data,                 \
                                 const array<std::int64_t>& origin_column_indices, \
                                 const array<std::int64_t>& origin_row_offsets,    \
                                 array<BlockData>& data,                           \
                                 array<std::int64_t>& column_indices,              \
                                 array<std::int64_t>& row_offsets,                 \
                                 alloc_kind requested_alloc_kind,                  \
                                 bool preserve_mutability);

#define INSTANTIATE_HOST_POLICY(Data) INSTANTIATE(detail::default_host_policy, Data)

INSTANTIATE_HOST_POLICY(float)
INSTANTIATE_HOST_POLICY(double)
INSTANTIATE_HOST_POLICY(std::int32_t)

} // namespace oneapi::dal::backend
