#ifndef PROTO_TXT_H
#define PROTO_TXT_H

namespace fallout {

extern const char* const anim_code_strs[];

int proto_save_text(int pid, const char* dir);
int proto_load_text(int pid);
int proto_read_text_fid(const char* text, int* outFid, int defaultType);

} // namespace fallout

#endif /* PROTO_TXT_H */
