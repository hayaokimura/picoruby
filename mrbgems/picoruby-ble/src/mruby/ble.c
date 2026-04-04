#include "mruby.h"
#include "mruby/presym.h"
#include "mruby/string.h"
#include "mruby/hash.h"
#include "mruby/array.h"

static mrb_state *_mrb = NULL;
static mrb_value write_values;
static mrb_value read_values;

void
BLE_push_event(uint8_t *data, uint16_t size)
{
  if (packet_mutex) return;
  packet_mutex = true;
  packet_flag = true;
  packet_size = size;
  if (packet != NULL) {
    mrb_free(_mrb, packet);
  }
  packet = mrb_malloc(_mrb, packet_size);
  memcpy(packet, data, packet_size);
  packet_mutex = false;
}

void
BLE_heartbeat(void)
{
  if (packet_mutex) return;
  heatbeat_flag = true;
}

int
BLE_write_data(uint16_t att_handle, const uint8_t *data, uint16_t size)
{
  if (att_handle == 0 || size == 0 || _mrb == NULL || mrb_hash_p(write_values) == false) {
    return -1;
  }
  mrb_value key = mrb_fixnum_value(att_handle);
  mrb_value write_value = mrb_str_new(_mrb, (const char *)data, size);
  write_values_mutex = true;
  mrb_value queue = mrb_hash_get(_mrb, write_values, key);
  if (!mrb_array_p(queue)) {
    queue = mrb_ary_new_capa(_mrb, 4);
    mrb_hash_set(_mrb, write_values, key, queue);
  }
  mrb_ary_push(_mrb, queue, write_value);
  write_values_mutex = false;
  return 0;
}

int
BLE_read_data(BLE_read_value_t *read_value)
{
  if (_mrb == NULL || mrb_hash_p(read_values) == false) return -1;
  mrb_value value = mrb_hash_get(_mrb, read_values, mrb_fixnum_value(read_value->att_handle));
  if (mrb_string_p(value) == false) return -1;
  read_value->data = (uint8_t *)RSTRING_PTR(value);
  read_value->size = (uint16_t)RSTRING_LEN(value);
  return 0;
}


static mrb_value
mrb_pop_heartbeat(mrb_state *mrb, mrb_value self)
{
  if (heatbeat_flag) {
    heatbeat_flag = false;
    return mrb_true_value();
  }
  return mrb_false_value();
}

static mrb_value
mrb_pop_packet(mrb_state *mrb, mrb_value self)
{
  mrb_value packet_value = mrb_nil_value();
  if (packet_mutex || !packet_flag) return packet_value;
  packet_mutex = true;
  packet_flag = false;
  packet_value = mrb_str_new(mrb, (const char *)packet, packet_size);
  mrb_free(mrb, packet);
  packet = NULL;
  packet_mutex = false;
  return packet_value;
}

static mrb_value
mrb_pop_write_value(mrb_state *mrb, mrb_value self)
{
  if (write_values_mutex) return mrb_nil_value();
  mrb_int handle;
  mrb_get_args(mrb, "i", &handle);
  mrb_value key = mrb_fixnum_value(handle);
  mrb_value queue = mrb_hash_get(mrb, write_values, key);
  if (!mrb_array_p(queue) || RARRAY_LEN(queue) == 0) {
    return mrb_nil_value();
  }
  return mrb_ary_shift(mrb, queue);
}

static mrb_value
mrb_push_read_value(mrb_state *mrb, mrb_value self)
{
  mrb_int handle;
  mrb_value read_value;
  mrb_get_args(mrb, "iS", &handle, &read_value);
  mrb_hash_set(mrb, read_values, mrb_fixnum_value(handle), read_value);
  return read_value;
}

static mrb_value
mrb__init(mrb_state *mrb, mrb_value self)
{
  _mrb = mrb;
  write_values = mrb_hash_new(mrb);
  mrb_gc_register(mrb, write_values);
  read_values = mrb_hash_new(mrb);
  mrb_gc_register(mrb, read_values);

  mrb_value profile;
  const uint8_t *profile_data;
  mrb_get_args(mrb, "o", &profile);

  if (mrb_string_p(profile)) {
    /* Protect profile_data from GC */
    //mrbc_incref(&v[1]);
    profile_data = (const uint8_t *)RSTRING_PTR(profile);
  } else if (mrb_nil_p(profile)) {
    profile_data = NULL;
  } else {
    mrb_raise(mrb, E_TYPE_ERROR, "BLE._init: wrong argument type");
  }
  int ble_role;
  mrb_value role = mrb_iv_get(mrb, self, MRB_IVSYM(role));
  switch (mrb_symbol(role)) {
    case MRB_SYM(central):
      ble_role = BLE_ROLE_CENTRAL;
      break;
    case MRB_SYM(peripheral):
      ble_role = BLE_ROLE_PERIPHERAL;
      break;
    case MRB_SYM(observer):
      ble_role = BLE_ROLE_OBSERVER;
      break;
    case MRB_SYM(broadcaster):
      ble_role = BLE_ROLE_BROADCASTER;
      break;
    default:
      mrb_raise(mrb, E_TYPE_ERROR, "BLE._init: wrong role type");
  }
  if (BLE_init(profile_data, ble_role) < 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "BLE init failed");
  }
  return self;
}

static mrb_value
mrb_hci_power_control(mrb_state *mrb, mrb_value self)
{
  mrb_int power_mode;
  mrb_get_args(mrb, "i", &power_mode);
  BLE_hci_power_control((uint8_t)power_mode);
  return mrb_fixnum_value(0);
}

static mrb_value
mrb_gap_local_bd_addr(mrb_state *mrb, mrb_value self)
{
  uint8_t addr[6];
  BLE_gap_local_bd_addr(addr);
  return mrb_str_new(mrb, (const char *)addr, 6);
}

static mrb_value
mrb_sm_request_pairing(mrb_state *mrb, mrb_value self)
{
  mrb_int conn_handle;
  mrb_get_args(mrb, "i", &conn_handle);
  BLE_sm_request_pairing((uint16_t)conn_handle);
  return mrb_nil_value();
}

static mrb_value
mrb_le_device_db_count(mrb_state *mrb, mrb_value self)
{
  return mrb_fixnum_value(BLE_le_device_db_count());
}

static mrb_value
mrb_le_device_db_remove(mrb_state *mrb, mrb_value self)
{
  mrb_int index;
  mrb_get_args(mrb, "i", &index);
  BLE_le_device_db_remove((int)index);
  return mrb_nil_value();
}

static mrb_value
mrb_le_device_db_info(mrb_state *mrb, mrb_value self)
{
  mrb_int index;
  mrb_get_args(mrb, "i", &index);
  uint8_t addr_type;
  uint8_t addr[6];
  BLE_le_device_db_info((int)index, &addr_type, addr);
  mrb_value ary = mrb_ary_new_capa(mrb, 2);
  mrb_ary_push(mrb, ary, mrb_fixnum_value(addr_type));
  mrb_ary_push(mrb, ary, mrb_str_new(mrb, (const char *)addr, 6));
  return ary;
}

void
mrb_picoruby_ble_gem_init(mrb_state* mrb)
{
  struct RClass *class_BLE = mrb_define_class_id(mrb, MRB_SYM(BLE), mrb->object_class);

  mrb_define_method_id(mrb, class_BLE, MRB_SYM(_init), mrb__init, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_BLE, MRB_SYM(hci_power_control), mrb_hci_power_control, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_BLE, MRB_SYM(gap_local_bd_addr), mrb_gap_local_bd_addr, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_BLE, MRB_SYM(pop_write_value), mrb_pop_write_value, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_BLE, MRB_SYM(push_read_value), mrb_push_read_value, MRB_ARGS_REQ(2));
  mrb_define_method_id(mrb, class_BLE, MRB_SYM(pop_heartbeat), mrb_pop_heartbeat, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_BLE, MRB_SYM(pop_packet), mrb_pop_packet, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_BLE, MRB_SYM(sm_request_pairing),  mrb_sm_request_pairing,  MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_BLE, MRB_SYM(le_device_db_count),  mrb_le_device_db_count,  MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_BLE, MRB_SYM(le_device_db_remove), mrb_le_device_db_remove, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_BLE, MRB_SYM(le_device_db_info),   mrb_le_device_db_info,   MRB_ARGS_REQ(1));

  mrb_init_class_BLE_Peripheral(mrb, class_BLE);
  mrb_init_class_BLE_Broadcaster(mrb, class_BLE);
  mrb_init_class_BLE_Central(mrb, class_BLE);
}

void
mrb_picoruby_ble_gem_final(mrb_state* mrb)
{
}
