#ifndef FALLOUT_MAPPER_MP_UTILS_H_
#define FALLOUT_MAPPER_MP_UTILS_H_

namespace fallout {

void mapperShowTimedMsg(const char* msg);
bool mapperYesNoDialog(const char* msg);
void mapperShowMessage(const char* msg);

// Resolves an art FID to its filename without extension, falling back to "None".
void art_name_no_ext(int fid, char* buf);

} // namespace fallout

#endif /* FALLOUT_MAPPER_MP_UTILS_H_ */
