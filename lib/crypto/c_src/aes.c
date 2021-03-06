/*
 * %CopyrightBegin%
 *
 * Copyright Ericsson AB 2010-2018. All Rights Reserved.
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
 *
 * %CopyrightEnd%
 */

#include "aes.h"
#include "cipher.h"

ERL_NIF_TERM aes_cfb_8_crypt(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{/* (Key, IVec, Data, IsEncrypt) */
     ErlNifBinary key, ivec, text;
     AES_KEY aes_key;
     unsigned char ivec_clone[16]; /* writable copy */
     int new_ivlen = 0;
     ERL_NIF_TERM ret;
     unsigned char *outp;

     CHECK_NO_FIPS_MODE();

     ASSERT(argc == 4);

     if (!enif_inspect_iolist_as_binary(env, argv[0], &key))
         goto bad_arg;
     if (key.size != 16 && key.size != 24 && key.size != 32)
         goto bad_arg;
     if (!enif_inspect_binary(env, argv[1], &ivec))
         goto bad_arg;
     if (ivec.size != 16)
         goto bad_arg;
     if (!enif_inspect_iolist_as_binary(env, argv[2], &text))
         goto bad_arg;

     memcpy(ivec_clone, ivec.data, 16);

     /* NOTE: This function returns 0 on success unlike most OpenSSL functions */
     if (AES_set_encrypt_key(key.data, (int)key.size * 8, &aes_key) != 0)
         goto err;
     if ((outp = enif_make_new_binary(env, text.size, &ret)) == NULL)
         goto err;
     AES_cfb8_encrypt((unsigned char *) text.data,
                      outp,
                      text.size, &aes_key, ivec_clone, &new_ivlen,
                      (argv[3] == atom_true));
     CONSUME_REDS(env,text);
     return ret;

 bad_arg:
 err:
     return enif_make_badarg(env);
}

ERL_NIF_TERM aes_cfb_128_crypt_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{/* (Key, IVec, Data, IsEncrypt) */
    ErlNifBinary key, ivec, text;
    AES_KEY aes_key;
    unsigned char ivec_clone[16]; /* writable copy */
    int new_ivlen = 0;
    ERL_NIF_TERM ret;
    unsigned char *outp;

    ASSERT(argc == 4);

    if (!enif_inspect_iolist_as_binary(env, argv[0], &key))
        goto bad_arg;
    if (key.size != 16 && key.size != 24 && key.size != 32)
        goto bad_arg;
    if (!enif_inspect_binary(env, argv[1], &ivec))
        goto bad_arg;
    if (ivec.size != 16)
        goto bad_arg;
    if (!enif_inspect_iolist_as_binary(env, argv[2], &text))
        goto bad_arg;

    memcpy(ivec_clone, ivec.data, 16);

    /* NOTE: This function returns 0 on success unlike most OpenSSL functions */
    if (AES_set_encrypt_key(key.data, (int)key.size * 8, &aes_key) != 0)
        goto err;

    if ((outp = enif_make_new_binary(env, text.size, &ret)) == NULL)
        goto err;
    AES_cfb128_encrypt((unsigned char *) text.data,
                       outp,
                       text.size, &aes_key, ivec_clone, &new_ivlen,
                       (argv[3] == atom_true));
    CONSUME_REDS(env,text);
    return ret;

 bad_arg:
 err:
    return enif_make_badarg(env);
}

ERL_NIF_TERM aes_ige_crypt_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{/* (Key, IVec, Data, IsEncrypt) */
#ifdef HAVE_AES_IGE
    ErlNifBinary key_bin, ivec_bin, data_bin;
    AES_KEY aes_key;
    unsigned char ivec[32];
    int type;
    unsigned char* ret_ptr;
    ERL_NIF_TERM ret;

    CHECK_NO_FIPS_MODE();

    ASSERT(argc == 4);

    if (!enif_inspect_iolist_as_binary(env, argv[0], &key_bin))
        goto bad_arg;
    if (key_bin.size != 16 && key_bin.size != 32)
        goto bad_arg;
    if (!enif_inspect_binary(env, argv[1], &ivec_bin))
        goto bad_arg;
    if (ivec_bin.size != 32)
        goto bad_arg;
    if (!enif_inspect_iolist_as_binary(env, argv[2], &data_bin))
        goto bad_arg;
    if (data_bin.size % 16 != 0)
        goto bad_arg;

    if (argv[3] == atom_true) {
       type = AES_ENCRYPT;
       /* NOTE: This function returns 0 on success unlike most OpenSSL functions */
       if (AES_set_encrypt_key(key_bin.data, (int)key_bin.size * 8, &aes_key) != 0)
           goto err;
    }
    else {
       type = AES_DECRYPT;
       /* NOTE: This function returns 0 on success unlike most OpenSSL functions */
       if (AES_set_decrypt_key(key_bin.data, (int)key_bin.size * 8, &aes_key) != 0)
           goto err;
    }

    if ((ret_ptr = enif_make_new_binary(env, data_bin.size, &ret)) == NULL)
        goto err;

    memcpy(ivec, ivec_bin.data, 32); /* writable copy */

    AES_ige_encrypt(data_bin.data, ret_ptr, data_bin.size, &aes_key, ivec, type);

    CONSUME_REDS(env,data_bin);
    return ret;

 bad_arg:
 err:
    return enif_make_badarg(env);

#else
    return atom_notsup;
#endif
}


/* Initializes state for ctr streaming (de)encryption
*/
#ifdef HAVE_EVP_AES_CTR
ERL_NIF_TERM aes_ctr_stream_init(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{/* (Key, IVec) */
    ErlNifBinary     key_bin, ivec_bin;
    struct evp_cipher_ctx *ctx = NULL;
    const EVP_CIPHER *cipher;
    ERL_NIF_TERM     ret;

    ASSERT(argc == 2);

    if (!enif_inspect_iolist_as_binary(env, argv[0], &key_bin))
        goto bad_arg;
    if (!enif_inspect_binary(env, argv[1], &ivec_bin))
        goto bad_arg;
    if (ivec_bin.size != 16)
        goto bad_arg;

    switch (key_bin.size)
    {
    case 16:
        cipher = EVP_aes_128_ctr();
        break;
    case 24:
        cipher = EVP_aes_192_ctr();
        break;
    case 32:
        cipher = EVP_aes_256_ctr();
        break;
    default:
        goto bad_arg;
    }

    if ((ctx = enif_alloc_resource(evp_cipher_ctx_rtype, sizeof(struct evp_cipher_ctx))) == NULL)
        goto err;
    if ((ctx->ctx = EVP_CIPHER_CTX_new()) == NULL)
        goto err;

    if (EVP_CipherInit_ex(ctx->ctx, cipher, NULL,
                          key_bin.data, ivec_bin.data, 1) != 1)
        goto err;

    if (EVP_CIPHER_CTX_set_padding(ctx->ctx, 0) != 1)
        goto err;

    ret = enif_make_resource(env, ctx);
    goto done;

 bad_arg:
    return enif_make_badarg(env);

 err:
    ret = enif_make_badarg(env);

 done:
    if (ctx)
        enif_release_resource(ctx);
    return ret;
}

ERL_NIF_TERM aes_ctr_stream_encrypt(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{/* (Context, Data) */
    struct evp_cipher_ctx *ctx = NULL, *new_ctx = NULL;
    ErlNifBinary   data_bin;
    ERL_NIF_TERM   ret, cipher_term;
    unsigned char  *out;
    int            outl = 0;

    ASSERT(argc == 2);

    if (!enif_get_resource(env, argv[0], evp_cipher_ctx_rtype, (void**)&ctx))
        goto bad_arg;
    if (!enif_inspect_iolist_as_binary(env, argv[1], &data_bin))
        goto bad_arg;
    if (data_bin.size > INT_MAX)
        goto bad_arg;

    if ((new_ctx = enif_alloc_resource(evp_cipher_ctx_rtype, sizeof(struct evp_cipher_ctx))) == NULL)
        goto err;
    if ((new_ctx->ctx = EVP_CIPHER_CTX_new()) == NULL)
        goto err;

    if (EVP_CIPHER_CTX_copy(new_ctx->ctx, ctx->ctx) != 1)
        goto err;

    if ((out = enif_make_new_binary(env, data_bin.size, &cipher_term)) == NULL)
        goto err;

    if (EVP_CipherUpdate(new_ctx->ctx, out, &outl, data_bin.data, (int)data_bin.size) != 1)
        goto err;
    ASSERT(outl >= 0 && (size_t)outl == data_bin.size);

    ret = enif_make_tuple2(env, enif_make_resource(env, new_ctx), cipher_term);
    CONSUME_REDS(env,data_bin);
    goto done;

 bad_arg:
    return enif_make_badarg(env);

 err:
    ret = enif_make_badarg(env);

 done:
    if (new_ctx)
        enif_release_resource(new_ctx);
    return ret;
}

#else /* if not HAVE_EVP_AES_CTR */

ERL_NIF_TERM aes_ctr_stream_init(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{/* (Key, IVec) */
    ErlNifBinary key_bin, ivec_bin;
    ERL_NIF_TERM ecount_bin;
    unsigned char *outp;

    ASSERT(argc == 2);

    if (!enif_inspect_iolist_as_binary(env, argv[0], &key_bin))
        goto bad_arg;
    if (key_bin.size != 16 && key_bin.size != 24 && key_bin.size != 32)
        goto bad_arg;
    if (!enif_inspect_binary(env, argv[1], &ivec_bin))
        goto bad_arg;
    if (ivec_bin.size != 16)
        goto bad_arg;

    if ((outp = enif_make_new_binary(env, AES_BLOCK_SIZE, &ecount_bin)) == NULL)
        goto err;

    memset(outp, 0, AES_BLOCK_SIZE);

    return enif_make_tuple4(env, argv[0], argv[1], ecount_bin, enif_make_int(env, 0));

 bad_arg:
 err:
    return enif_make_badarg(env);
}

ERL_NIF_TERM aes_ctr_stream_encrypt(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{/* ({Key, IVec, ECount, Num}, Data) */
    ErlNifBinary key_bin, ivec_bin, text_bin, ecount_bin;
    AES_KEY aes_key;
    unsigned int num;
    ERL_NIF_TERM ret, num2_term, cipher_term, ivec2_term, ecount2_term, new_state_term;
    int state_arity;
    const ERL_NIF_TERM *state_term;
    unsigned char * ivec2_buf;
    unsigned char * ecount2_buf;
    unsigned char *outp;

    ASSERT(argc == 2);

    if (!enif_get_tuple(env, argv[0], &state_arity, &state_term))
        goto bad_arg;
    if (state_arity != 4)
        goto bad_arg;
    if (!enif_inspect_iolist_as_binary(env, state_term[0], &key_bin))
        goto bad_arg;
    if (key_bin.size > INT_MAX / 8)
        goto bad_arg;
    if (!enif_inspect_binary(env, state_term[1], &ivec_bin))
        goto bad_arg;
    if (ivec_bin.size != 16)
        goto bad_arg;
    if (!enif_inspect_binary(env, state_term[2], &ecount_bin))
        goto bad_arg;
    if (ecount_bin.size != AES_BLOCK_SIZE)
        goto bad_arg;
    if (!enif_get_uint(env, state_term[3], &num))
        goto bad_arg;
    if (!enif_inspect_iolist_as_binary(env, argv[1], &text_bin))
        goto bad_arg;

    /* NOTE: This function returns 0 on success unlike most OpenSSL functions */
    if (AES_set_encrypt_key(key_bin.data, (int)key_bin.size * 8, &aes_key) != 0)
        goto bad_arg;

    if ((ivec2_buf = enif_make_new_binary(env, ivec_bin.size, &ivec2_term)) == NULL)
        goto err;
    if ((ecount2_buf = enif_make_new_binary(env, ecount_bin.size, &ecount2_term)) == NULL)
        goto err;

    memcpy(ivec2_buf, ivec_bin.data, 16);
    memcpy(ecount2_buf, ecount_bin.data, ecount_bin.size);

    if ((outp = enif_make_new_binary(env, text_bin.size, &cipher_term)) == NULL)
        goto err;

    AES_ctr128_encrypt((unsigned char *) text_bin.data,
                       outp,
		       text_bin.size, &aes_key, ivec2_buf, ecount2_buf, &num);

    num2_term = enif_make_uint(env, num);
    new_state_term = enif_make_tuple4(env, state_term[0], ivec2_term, ecount2_term, num2_term);
    ret = enif_make_tuple2(env, new_state_term, cipher_term);
    CONSUME_REDS(env,text_bin);
    return ret;

 bad_arg:
 err:
    return enif_make_badarg(env);
}
#endif /* !HAVE_EVP_AES_CTR */

#ifdef HAVE_GCM_EVP_DECRYPT_BUG
ERL_NIF_TERM aes_gcm_decrypt_NO_EVP(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{/* (Type,Key,Iv,AAD,In,Tag) */
    GCM128_CONTEXT *ctx = NULL;
    ErlNifBinary key, iv, aad, in, tag;
    AES_KEY aes_key;
    unsigned char *outp;
    ERL_NIF_TERM out, ret;

    ASSERT(argc == 6);

    if (!enif_inspect_iolist_as_binary(env, argv[1], &key))
        goto bad_arg;
    if (key.size > INT_MAX / 8)
        goto bad_arg;
    if (!enif_inspect_binary(env, argv[2], &iv))
        goto bad_arg;
    if (iv.size == 0)
        goto bad_arg;
    if (!enif_inspect_iolist_as_binary(env, argv[3], &aad))
        goto bad_arg;
    if (!enif_inspect_iolist_as_binary(env, argv[4], &in))
        goto bad_arg;
    if (!enif_inspect_iolist_as_binary(env, argv[5], &tag))
        goto bad_arg;

    /* NOTE: This function returns 0 on success unlike most OpenSSL functions */
    if (AES_set_encrypt_key(key.data, (int)key.size * 8, &aes_key) != 0)
        goto bad_arg;

    if ((ctx = CRYPTO_gcm128_new(&aes_key, (block128_f)AES_encrypt)) == NULL)
        goto err;

    CRYPTO_gcm128_setiv(ctx, iv.data, iv.size);

    /* NOTE: This function returns 0 on success unlike most OpenSSL functions */
    if (CRYPTO_gcm128_aad(ctx, aad.data, aad.size) != 0)
        goto err;

    if ((outp = enif_make_new_binary(env, in.size, &out)) == NULL)
        goto err;

    /* NOTE: This function returns 0 on success unlike most OpenSSL functions */
    if (CRYPTO_gcm128_decrypt(ctx, in.data, outp, in.size) != 0)
        goto err;

    /* calculate and check the tag */
    /* NOTE: This function returns 0 on success unlike most OpenSSL functions */
    if (CRYPTO_gcm128_finish(ctx, tag.data, tag.size) != 0)
        goto err;

    CONSUME_REDS(env, in);
    ret = out;
    goto done;

 bad_arg:
    ret = enif_make_badarg(env);
    goto done;

 err:
    ret = atom_error;

 done:
    if (ctx)
        CRYPTO_gcm128_release(ctx);
    return ret;
}
#endif /* HAVE_GCM_EVP_DECRYPT_BUG */

