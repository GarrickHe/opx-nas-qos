/*
 * Copyright (c) 2018 Dell Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * THIS CODE IS PROVIDED ON AN *AS IS* BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT
 * LIMITATION ANY IMPLIED WARRANTIES OR CONDITIONS OF TITLE, FITNESS
 * FOR A PARTICULAR PURPOSE, MERCHANTABLITY OR NON-INFRINGEMENT.
 *
 * See the Apache Version 2.0 License for specific language governing
 * permissions and limitations under the License.
 */

/*!
 * \file   nas_qos_scheduler.cpp
 * \brief  NAS QOS Scheduler Object
 * \date   05-2015
 * \author
 */

#include "event_log.h"
#include "std_assert.h"
#include "nas_qos_scheduler.h"
#include "dell-base-qos.h"
#include "nas_qos_switch.h"

nas_qos_scheduler::nas_qos_scheduler (nas_qos_switch* p_switch)
           : base_obj_t (p_switch)
{
    scheduler_id = 0;
    memset(&cfg, 0, sizeof(cfg));
    memset(profile_name, 0, sizeof(profile_name));
}

const nas_qos_switch& nas_qos_scheduler::get_switch()
{
    return static_cast<const nas_qos_switch&> (base_obj_t::get_switch());
}


void* nas_qos_scheduler::alloc_fill_ndi_obj (nas::mem_alloc_helper_t& m)
{
    // NAS Qos scheduler does not allocate memory to save the incoming tentative attributes
    return this;
}

bool nas_qos_scheduler::push_create_obj_to_npu (npu_id_t npu_id,
                                     void* ndi_obj)
{
    ndi_obj_id_t ndi_scheduler_id;
    t_std_error rc;

    EV_LOGGING(QOS, DEBUG, "NAS-QOS", "Creating obj on NPU %d", npu_id);

    nas_qos_scheduler * nas_qos_scheduler_p = static_cast<nas_qos_scheduler*> (ndi_obj);

    // form attr_list
    std::vector<uint64_t> attr_list;
    attr_list.resize(_set_attributes.len());

    uint_t num_attr = 0;
    for (auto attr_id: _set_attributes) {
        attr_list[num_attr++] = attr_id;
    }

    if ((rc = ndi_qos_create_scheduler_profile (npu_id,
                                      &attr_list[0],
                                      num_attr,
                                      &nas_qos_scheduler_p->cfg,
                                      &ndi_scheduler_id))
            != STD_ERR_OK)
    {
        throw nas::base_exception {rc, __PRETTY_FUNCTION__,
            "NDI QoS Scheduler Create Failed"};
    }
    // Cache the new Scheduler ID generated by NDI
    set_ndi_obj_id(npu_id, ndi_scheduler_id);

    return true;

}


bool nas_qos_scheduler::push_delete_obj_to_npu (npu_id_t npu_id)
{
    t_std_error rc;

    EV_LOGGING(QOS, DEBUG, "NAS-QOS", "Deleting obj on NPU %d", npu_id);

    if ((rc = ndi_qos_delete_scheduler_profile(npu_id, ndi_obj_id(npu_id)))
        != STD_ERR_OK)
    {
        throw nas::base_exception {rc, __PRETTY_FUNCTION__,
            "NDI Scheduler Delete Failed"};
    }

    return true;
}

bool nas_qos_scheduler::is_leaf_attr (nas_attr_id_t attr_id)
{
    // Table of function pointers to handle modify of Qos scheduler
    // attributes.
    static const std::unordered_map <BASE_QOS_SCHEDULER_PROFILE_t,
                                     bool,
                                     std::hash<int>>
        _leaf_attr_map =
    {
        // modifiable objects
        {BASE_QOS_SCHEDULER_PROFILE_ALGORITHM,    true},
        {BASE_QOS_SCHEDULER_PROFILE_WEIGHT,        true},
        {BASE_QOS_SCHEDULER_PROFILE_METER_TYPE, true},
        {BASE_QOS_SCHEDULER_PROFILE_MIN_RATE,   true},
        {BASE_QOS_SCHEDULER_PROFILE_MIN_BURST,  true},
        {BASE_QOS_SCHEDULER_PROFILE_MAX_RATE,   true},
        {BASE_QOS_SCHEDULER_PROFILE_MAX_BURST,  true},
        {BASE_QOS_SCHEDULER_PROFILE_NPU_ID_LIST,true},
        {BASE_QOS_SCHEDULER_PROFILE_NAME,       true},

        //The NPU ID list attribute is handled by the base object itself.
    };

    return (_leaf_attr_map.at(static_cast<BASE_QOS_SCHEDULER_PROFILE_t>(attr_id)));
}

bool nas_qos_scheduler::push_leaf_attr_to_npu (nas_attr_id_t attr_id,
                                           npu_id_t npu_id)
{
    t_std_error rc = STD_ERR_OK;

    EV_LOGGING(QOS, DEBUG, "QOS", "Modifying npu: %d, attr_id %lu",
                    npu_id, attr_id);


    switch (attr_id) {
    case BASE_QOS_SCHEDULER_PROFILE_ALGORITHM:
    case BASE_QOS_SCHEDULER_PROFILE_WEIGHT:
    case BASE_QOS_SCHEDULER_PROFILE_METER_TYPE:
    case BASE_QOS_SCHEDULER_PROFILE_MIN_RATE:
    case BASE_QOS_SCHEDULER_PROFILE_MIN_BURST:
    case BASE_QOS_SCHEDULER_PROFILE_MAX_RATE:
    case BASE_QOS_SCHEDULER_PROFILE_MAX_BURST:
        if ((rc = ndi_qos_set_scheduler_profile_attr(npu_id, ndi_obj_id(npu_id),
                                           (BASE_QOS_SCHEDULER_PROFILE_t)attr_id,
                                           &cfg))
                != STD_ERR_OK)
            {
                throw nas::base_exception {rc, __PRETTY_FUNCTION__,
                    "NDI attribute Set Failed"};
            }
        break;

    case BASE_QOS_SCHEDULER_PROFILE_NAME:
        break; // do not need to pass to SAI

    case BASE_QOS_SCHEDULER_PROFILE_NPU_ID_LIST:
        // handled separately, not here
        break;

    default:
            STD_ASSERT (0); //non-modifiable object
    }

    return true;
}

