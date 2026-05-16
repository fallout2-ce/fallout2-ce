#ifndef FALLOUT_MAPPER_MP_PROTO_H_
#define FALLOUT_MAPPER_MP_PROTO_H_

namespace fallout {

class Object;

union Proto;
typedef int (*protoChooseFidCallback)(Proto* proto);
typedef int (*protoChooseAddCallback)(int pid, int count);

extern char* proto_builder_name;
extern bool can_modify_protos;

void init_mapper_protos();
const char* proto_wall_light_str(int flags);
int proto_pick_ai_packet(int* value);
int proto_build_all_type(int type);
int proto_build_all_type_binary(int type);
void rebuild_spray_tools();
void rebuild_binary();
void art_to_protos();
void swap_protos();
int protoEdit(int protoId);
int protoChooseMultiPids(int pidType, protoChooseFidCallback fidFunc, protoChooseAddCallback addFunc);
// protoInstEdit moved to mp_instance.h

} // namespace fallout

#endif /* FALLOUT_MAPPER_MP_PROTO_H_ */
