/*
Generated by LwipMibCompiler
*/

#include "lwip/apps/snmp_opts.h"
#if LWIP_SNMP

#include "sensorhub.h"
#include "lwip/apps/snmp.h"
#include "lwip/apps/snmp_core.h"
#include "lwip/apps/snmp_scalar.h"
#include "lwip/apps/snmp_table.h"


/* --- sensorHubMIB  ----------------------------------------------------- */
static snmp_err_t shsensortable_get_instance(const u32_t *column, const u32_t *row_oid, u8_t row_oid_len, struct snmp_node_instance *cell_instance);
static snmp_err_t shsensortable_get_next_instance(const u32_t *column, struct snmp_obj_id *row_oid, struct snmp_node_instance *cell_instance);
static s16_t shsensortable_get_value(struct snmp_node_instance *cell_instance, void *value);
static const struct snmp_table_col_def shsensortable_columns[] = {
  {2, SNMP_ASN1_TYPE_OCTET_STRING, SNMP_NODE_INSTANCE_READ_ONLY}, /* shSensorName */ 
};
static const struct snmp_table_node shsensortable = SNMP_TABLE_CREATE(1, shsensortable_columns, shsensortable_get_instance, shsensortable_get_next_instance, shsensortable_get_value, NULL, NULL);

static snmp_err_t shmeasurementtable_get_instance(const u32_t *column, const u32_t *row_oid, u8_t row_oid_len, struct snmp_node_instance *cell_instance);
static snmp_err_t shmeasurementtable_get_next_instance(const u32_t *column, struct snmp_obj_id *row_oid, struct snmp_node_instance *cell_instance);
static s16_t shmeasurementtable_get_value(struct snmp_node_instance *cell_instance, void *value);
static const struct snmp_table_col_def shmeasurementtable_columns[] = {
  {1, SNMP_ASN1_TYPE_INTEGER, SNMP_NODE_INSTANCE_READ_ONLY}, /* shMeasurementType */ 
  {2, SNMP_ASN1_TYPE_INTEGER, SNMP_NODE_INSTANCE_READ_ONLY}, /* shMeasurementValue */ 
};
static const struct snmp_table_node shmeasurementtable = SNMP_TABLE_CREATE(2, shmeasurementtable_columns, shmeasurementtable_get_instance, shmeasurementtable_get_next_instance, shmeasurementtable_get_value, NULL, NULL);

static s16_t shSetExample_get_value(struct snmp_node_instance *instance, void *value);
static snmp_err_t shSetExample_set_value(struct snmp_node_instance *instance, u16_t len, void *value);
static const struct snmp_scalar_node shsetexample_scalar = SNMP_SCALAR_CREATE_NODE(3, SNMP_NODE_INSTANCE_READ_WRITE, SNMP_ASN1_TYPE_INTEGER, shSetExample_get_value, snmp_set_test_ok, shSetExample_set_value);

static const struct snmp_node *const sensorhubmib_subnodes[] = {
  &shsensortable.node.node,
  &shmeasurementtable.node.node,
  &shsetexample_scalar.node.node
};
static const struct snmp_tree_node sensorhubmib_root = SNMP_CREATE_TREE_NODE(1, sensorhubmib_subnodes);
static const u32_t sensorhubmib_base_oid[] = {1,3,6,1,4,1,58049,1};
const struct snmp_mib sensorhubmib = {sensorhubmib_base_oid, LWIP_ARRAYSIZE(sensorhubmib_base_oid), &sensorhubmib_root.node};



/*
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
LWIP MIB generator - preserved section begin
Code below is preserved on regeneration. Remove these comment lines to regenerate code.
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*/

/* --- sensorHubMIB  ----------------------------------------------------- */
static snmp_err_t shsensortable_get_instance(const u32_t *column, const u32_t *row_oid, u8_t row_oid_len, struct snmp_node_instance *cell_instance)
{
   /*
   The instance OID of this table consists of following (index) column(s):
    shSensorId (Integer, OID length = 1)
   */
   snmp_err_t err = SNMP_ERR_NOSUCHINSTANCE;

   if (row_oid_len == 1)
   {
      LWIP_UNUSED_ARG(column);
      LWIP_UNUSED_ARG(row_oid);
      LWIP_UNUSED_ARG(cell_instance);
      /*
      TODO: check if 'row_oid'/'row_oid_len' params contain a valid instance oid for a row
      If so, set 'err = SNMP_ERR_NOERROR;'
      
      snmp_oid_* methods may be used for easier processing of oid
      
      In order to avoid decoding OID a second time in subsequent get_value/set_test/set_value methods,
      you may store an arbitrary value (like a pointer to target value object) in 'cell_instance->reference'/'cell_instance->reference_len'.
      But be aware that not always a subsequent method is called -> Do NOT allocate memory here and try to release it in subsequent methods!
      
      You also may replace function pointers in 'cell_instance' param for get/test/set methods which contain the default values from table definition,
      in order to provide special methods, for the currently processed cell. Changed pointers are only valid for current request.
      */
   }
   return err;
}
static snmp_err_t shsensortable_get_next_instance(const u32_t *column, struct snmp_obj_id *row_oid, struct snmp_node_instance *cell_instance)
{
   /*
   The instance OID of this table consists of following (index) column(s):
    shSensorId (Integer, OID length = 1)
   */
   snmp_err_t err = SNMP_ERR_NOSUCHINSTANCE;

   LWIP_UNUSED_ARG(column);
   LWIP_UNUSED_ARG(row_oid);
   LWIP_UNUSED_ARG(cell_instance);
   /*
   TODO: analyze 'row_oid->id'/'row_oid->len' and return the subsequent row instance
   Be aware that 'row_oid->id'/'row_oid->len' must not point to a valid instance or have correct instance length.
   If 'row_oid->len' is 0, return the first instance. If 'row_oid->len' is longer than expected, cut superfluous OID parts.
   If a valid next instance is found, store it in 'row_oid->id'/'row_oid->len' and set 'err = SNMP_ERR_NOERROR;'
   
   snmp_oid_* methods may be used for easier processing of oid
   
   In order to avoid decoding OID a second time in subsequent get_value/set_test/set_value methods,
   you may store an arbitrary value (like a pointer to target value object) in 'cell_instance->reference'/'cell_instance->reference_len'.
   But be aware that not always a subsequent method is called -> Do NOT allocate memory here and try to release it in subsequent methods!
   
   You also may replace function pointers in 'cell_instance' param for get/test/set methods which contain the default values from table definition,
   in order to provide special methods, for the currently processed cell. Changed pointers are only valid for current request.
   */
   /*
   For easier processing and getting the next instance, you may use the 'snmp_next_oid_*' enumerator.
   Simply pass all known instance OID's to it and it returns the next valid one:
   
   struct snmp_next_oid_state state;
   struct snmp_obj_id result_buf;
   snmp_next_oid_init(&state, row_oid->id, row_oid->len, result_buf.id, SNMP_MAX_OBJ_ID_LEN);
   while ({not all instances passed}) {
     struct snmp_obj_id test_oid;
     {fill test_oid to create instance oid for next instance}
     snmp_next_oid_check(&state, test_oid.id, test_oid.len, {target_data_ptr});
   }
   if(state.status == SNMP_NEXT_OID_STATUS_SUCCESS) {
     snmp_oid_assign(row_oid, state.next_oid, state.next_oid_len);
     cell_instance->reference.ptr = state.reference; //==target_data_ptr, for usage in subsequent get/test/set
     err = SNMP_ERR_NOERROR;
   }
   */
   return err;
}
static s16_t shsensortable_get_value(struct snmp_node_instance *cell_instance, void *value)
{
   s16_t value_len;

   switch (SNMP_TABLE_GET_COLUMN_FROM_OID(cell_instance->instance_oid.id))
   {
      case 2:
         {
            /* shSensorName */
            u8_t *v = (u8_t *)value;

            /* TODO: take care that value with variable length fits into buffer: (value_len <= SNMP_MAX_VALUE_SIZE) */
            /* TODO: take care of len restrictions defined in MIB: ((value_len >= 0) && (value_len <= 255)) */
            /* TODO: put requested value to '*v' here */
            value_len = 0;
            LWIP_UNUSED_ARG(v);
         }
         break;
      default:
         {
            LWIP_DEBUGF(SNMP_MIB_DEBUG,("shsensortable_get_value(): unknown id: %"S32_F"\n", SNMP_TABLE_GET_COLUMN_FROM_OID(cell_instance->instance_oid.id)));
            value_len = 0;
         }
         break;
   }
   return value_len;
}

static snmp_err_t shmeasurementtable_get_instance(const u32_t *column, const u32_t *row_oid, u8_t row_oid_len, struct snmp_node_instance *cell_instance)
{
   /*
   The instance OID of this table consists of following (index) column(s):
    shSensorId (Integer, OID length = 1)
   */
   snmp_err_t err = SNMP_ERR_NOSUCHINSTANCE;

   if (row_oid_len == 1)
   {
      LWIP_UNUSED_ARG(column);
      LWIP_UNUSED_ARG(row_oid);
      LWIP_UNUSED_ARG(cell_instance);
      /*
      TODO: check if 'row_oid'/'row_oid_len' params contain a valid instance oid for a row
      If so, set 'err = SNMP_ERR_NOERROR;'
      
      snmp_oid_* methods may be used for easier processing of oid
      
      In order to avoid decoding OID a second time in subsequent get_value/set_test/set_value methods,
      you may store an arbitrary value (like a pointer to target value object) in 'cell_instance->reference'/'cell_instance->reference_len'.
      But be aware that not always a subsequent method is called -> Do NOT allocate memory here and try to release it in subsequent methods!
      
      You also may replace function pointers in 'cell_instance' param for get/test/set methods which contain the default values from table definition,
      in order to provide special methods, for the currently processed cell. Changed pointers are only valid for current request.
      */
   }
   return err;
}
static snmp_err_t shmeasurementtable_get_next_instance(const u32_t *column, struct snmp_obj_id *row_oid, struct snmp_node_instance *cell_instance)
{
   /*
   The instance OID of this table consists of following (index) column(s):
    shSensorId (Integer, OID length = 1)
   */
   snmp_err_t err = SNMP_ERR_NOSUCHINSTANCE;

   LWIP_UNUSED_ARG(column);
   LWIP_UNUSED_ARG(row_oid);
   LWIP_UNUSED_ARG(cell_instance);
   /*
   TODO: analyze 'row_oid->id'/'row_oid->len' and return the subsequent row instance
   Be aware that 'row_oid->id'/'row_oid->len' must not point to a valid instance or have correct instance length.
   If 'row_oid->len' is 0, return the first instance. If 'row_oid->len' is longer than expected, cut superfluous OID parts.
   If a valid next instance is found, store it in 'row_oid->id'/'row_oid->len' and set 'err = SNMP_ERR_NOERROR;'
   
   snmp_oid_* methods may be used for easier processing of oid
   
   In order to avoid decoding OID a second time in subsequent get_value/set_test/set_value methods,
   you may store an arbitrary value (like a pointer to target value object) in 'cell_instance->reference'/'cell_instance->reference_len'.
   But be aware that not always a subsequent method is called -> Do NOT allocate memory here and try to release it in subsequent methods!
   
   You also may replace function pointers in 'cell_instance' param for get/test/set methods which contain the default values from table definition,
   in order to provide special methods, for the currently processed cell. Changed pointers are only valid for current request.
   */
   /*
   For easier processing and getting the next instance, you may use the 'snmp_next_oid_*' enumerator.
   Simply pass all known instance OID's to it and it returns the next valid one:
   
   struct snmp_next_oid_state state;
   struct snmp_obj_id result_buf;
   snmp_next_oid_init(&state, row_oid->id, row_oid->len, result_buf.id, SNMP_MAX_OBJ_ID_LEN);
   while ({not all instances passed}) {
     struct snmp_obj_id test_oid;
     {fill test_oid to create instance oid for next instance}
     snmp_next_oid_check(&state, test_oid.id, test_oid.len, {target_data_ptr});
   }
   if(state.status == SNMP_NEXT_OID_STATUS_SUCCESS) {
     snmp_oid_assign(row_oid, state.next_oid, state.next_oid_len);
     cell_instance->reference.ptr = state.reference; //==target_data_ptr, for usage in subsequent get/test/set
     err = SNMP_ERR_NOERROR;
   }
   */
   return err;
}
static s16_t shmeasurementtable_get_value(struct snmp_node_instance *cell_instance, void *value)
{
   s16_t value_len;

   switch (SNMP_TABLE_GET_COLUMN_FROM_OID(cell_instance->instance_oid.id))
   {
      case 1:
         {
            /* shMeasurementType */
            s32_t *v = (s32_t *)value;

            /* TODO: put requested value to '*v' here */
            value_len = sizeof(s32_t);
            LWIP_UNUSED_ARG(v);
         }
         break;
      case 2:
         {
            /* shMeasurementValue */
            s32_t *v = (s32_t *)value;

            /* TODO: put requested value to '*v' here */
            value_len = sizeof(s32_t);
            LWIP_UNUSED_ARG(v);
         }
         break;
      default:
         {
            LWIP_DEBUGF(SNMP_MIB_DEBUG,("shmeasurementtable_get_value(): unknown id: %"S32_F"\n", SNMP_TABLE_GET_COLUMN_FROM_OID(cell_instance->instance_oid.id)));
            value_len = 0;
         }
         break;
   }
   return value_len;
}

static s16_t shSetExample_get_value(struct snmp_node_instance *instance, void *value)
{
   s16_t value_len;
   s32_t *v = (s32_t *)value;

   LWIP_UNUSED_ARG(instance);
   /* TODO: put requested value to '*v' here */
   value_len = sizeof(s32_t);
   LWIP_UNUSED_ARG(v);
   return value_len;
}
static snmp_err_t shSetExample_set_value(struct snmp_node_instance *instance, u16_t len, void *value)
{
   snmp_err_t err = SNMP_ERR_NOERROR;
   s32_t *v = (s32_t *)value;

   LWIP_UNUSED_ARG(instance);
   /* TODO: store new value contained in '*v' here */
   LWIP_UNUSED_ARG(v);
   LWIP_UNUSED_ARG(len);
   return err;
}

#endif /* LWIP_SNMP */