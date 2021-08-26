/* file: batch_result.cpp */
/*******************************************************************************
* Copyright 2014-2021 Intel Corporation
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

#include <jni.h>
#include "com_intel_daal_algorithms_normalization_minmax_Result.h"

#include "daal.h"

#include "com/intel/daal/common_helpers.h"

USING_COMMON_NAMESPACES()
using namespace daal::algorithms::normalization::minmax;

/*
 * Class:     com_intel_daal_algorithms_normalization_minmax_Result
 * Method:    cNewResult
 * Signature: ()J
 */
JNIEXPORT jlong JNICALL Java_com_intel_daal_algorithms_normalization_minmax_Result_cNewResult(JNIEnv * env, jobject thisObj)
{
    return jniArgument<normalization::minmax::Result>::newObj();
}

/*
 * Class:     com_intel_daal_algorithms_normalization_minmax_Result
 * Method:    cGetNormalizedData
 * Signature: (J)J
 */
JNIEXPORT jlong JNICALL Java_com_intel_daal_algorithms_normalization_minmax_Result_cGetNormalizedData(JNIEnv * env, jobject thisObj, jlong resAddr)
{
    return jniArgument<normalization::minmax::Result>::get<normalization::minmax::ResultId, NumericTable>(resAddr,
                                                                                                          normalization::minmax::normalizedData);
}

/*
 * Class:     com_intel_daal_algorithms_normalization_minmax_Result
 * Method:    cSetNormalizedData
 * Signature: (JJ)V
 */
JNIEXPORT void JNICALL Java_com_intel_daal_algorithms_normalization_minmax_Result_cSetNormalizedData(JNIEnv * env, jobject thisObj, jlong resAddr,
                                                                                                     jlong ntAddr)
{
    jniArgument<normalization::minmax::Result>::set<normalization::minmax::ResultId, NumericTable>(resAddr, normalization::minmax::normalizedData,
                                                                                                   ntAddr);
}