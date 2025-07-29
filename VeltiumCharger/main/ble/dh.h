#ifndef __DH_H__
#define __DH_H__

#include <cstdint>

void dh_generate_keys(uint8_t* public_key, uint8_t* private_key);
void dh_generate_shared_secret(const uint8_t* my_private_key, const uint8_t* their_public_key, uint8_t* shared_secret);

#endif // __DH_H__
