#include "dh.h"
#include "mbedtls/dhm.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"

void dh_generate_keys(uint8_t* public_key, uint8_t* private_key)
{
    mbedtls_dhm_context dhm;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    const char *pers = "dhm";

    mbedtls_dhm_init(&dhm);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);

    mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char *) pers, strlen(pers));

    // P and G are well-known DH parameters
    mbedtls_mpi_read_string(&dhm.P, 16, "FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD129024E088A67CC74020BBEA63B139B22514A08798E3404DDEF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7EDEE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3DC2007CB8A163BF0598DA48361C55D39A69163FA8FD24CF5F83655D23DCA3AD961C62F356208552BB9ED529077096966D670C354E4ABC9804F1746C08CA237327FFFFFFFFFFFFFFFF");
    mbedtls_mpi_read_string(&dhm.G, 16, "2");

    mbedtls_dhm_make_public(&dhm, (int) mbedtls_mpi_size(&dhm.P), public_key, mbedtls_mpi_size(&dhm.P), mbedtls_ctr_drbg_random, &ctr_drbg);
    mbedtls_mpi_write_binary(&dhm.X, private_key, mbedtls_mpi_size(&dhm.X));

    mbedtls_dhm_free(&dhm);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
}

void dh_generate_shared_secret(const uint8_t* my_private_key, const uint8_t* their_public_key, uint8_t* shared_secret)
{
    mbedtls_dhm_context dhm;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_context entropy;
    const char *pers = "dhm";

    mbedtls_dhm_init(&dhm);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);

    mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char *) pers, strlen(pers));

    mbedtls_mpi_read_string(&dhm.P, 16, "FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD129024E088A67CC74020BBEA63B139B22514A08798E3404DDEF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7EDEE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3DC2007CB8A163BF0598DA48361C55D39A69163FA8FD24CF5F83655D23DCA3AD961C62F356208552BB9ED529077096966D670C354E4ABC9804F1746C08CA237327FFFFFFFFFFFFFFFF");
    mbedtls_mpi_read_string(&dhm.G, 16, "2");

    mbedtls_mpi_read_binary(&dhm.X, my_private_key, mbedtls_mpi_size(&dhm.P));
    mbedtls_dhm_read_public(&dhm, their_public_key, mbedtls_mpi_size(&dhm.P));
    mbedtls_dhm_calc_secret(&dhm, shared_secret, mbedtls_mpi_size(&dhm.P), NULL, mbedtls_ctr_drbg_random, &ctr_drbg);

    mbedtls_dhm_free(&dhm);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
}
