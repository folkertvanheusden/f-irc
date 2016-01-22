/* Released under GPLv2 with exception for the OpenSSL library. See license.txt */
/* $Revision: 695 $ */

#include <errno.h>
#include <libintl.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <openssl/bio.h>
#include <openssl/conf.h>
#include <openssl/engine.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/md5.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#include "error.h"
#include "gen.h"
#include "mssl.h"
#include "tcp.h"
#include "io.h"
#include "http.h"
#include "utils.h"

typedef enum { NONE_AVAIL, ERROR, GOT_DATA } ssl_readwrite_status;

void init_ssl(void)
{
	SSL_library_init();
	SSL_load_error_strings();
	ERR_load_crypto_strings();
}

void shutdown_ssl(void)
{
	ERR_free_strings();

	ERR_remove_state(0);
	ENGINE_cleanup();
	CONF_modules_free();
	EVP_cleanup();
	CRYPTO_cleanup_all_ex_data();
}

typedef struct
{
	struct sockaddr_un addr;

	// parent info
	phtread_t *tid;
	int pfd;

	pthread_mutex_t recvlock;
	char *recv;
	int n_recv;

	// thread info
	SSL_CTX *ctx;
	SSL *ssl_h;
	BIO *s_bio;
	int tfd;
	int ifd; /* irc fd */

	pthread_mutex_t sendlock;
	char *tosend;
	int n_tosend;
}
ssl_variables;

void * ssl_thread(void *pars)
{
	ssl_variables *sv = (ssl_variables *)pars;

	/* resolve */
	sv -> tfd = socket(AF_UNIX, SOCK_STREAM, 0);

	// tcp connect
	// void connect_ssl(ssl_variables *sv, double timeout, char **msg)
}

ssl_variables *start_ssl_thread(const char *host, int port)
{
	ssl_variables *sv = (ssl_variables *)calloc(1, sizeof ssl_variables);

	// start_thread

	return sv;
}

SSL_CTX * initialize_ctx(BOOL ask_compression)
{
	/* create context */
	const SSL_METHOD *meth = SSLv23_method();

	SSL_CTX *ctx = SSL_CTX_new(meth);

#ifdef SSL_OP_NO_COMPRESSION
	if (!ask_compression)
		SSL_CTX_set_options(ctx, SSL_OP_NO_COMPRESSION);
#endif

	return ctx;
}

void end_ctx(SSL_CTX *in)
{
	SSL_CTX_free(in);
}

char close_ssl_connection(SSL *ssl_h)
{
	int rc = SSL_shutdown(ssl_h);

	if (!rc)
		rc = SSL_shutdown(ssl_h);

	return 0;
}

ssl_readwrite_status read_SSL(SSL *ssl_h, char *whereto, int len, int *n_read)
{
	*n_read = 0;

	while(len > 0)
	{
		int rc;

		rc = SSL_read(ssl_h, whereto, len);

		if (rc == -1)
		{
			int sge = -1;

			if (*n_read)
				return GOT_DATA;

			sge = SSL_get_error(rc);

			if (sge == SSL_ERROR_WANT_READ)
				return NO_DATA;
			if (sge == SSL_ERROR_WANT_WRITE)
				return NO_DATA;

			return ERROR;
		}

		if (rc == 0)
		{
			if (*n_read)
				return GOT_DATA;

			return ERROR;
		}

		whereto += rc;

		len -= rc;

		*n_read += rc;
	}

	return GOT_DATA;
}

ssl_readwrite_status WRITE_SSL(SSL *ssl_h, const char *wherefrom, int len)
{
	int cnt = len;

	while(len>0)
	{
		int rc;

		rc = SSL_write(ssl_h, wherefrom, len);
		if (rc == -1)
		{
			int sge = -1;

			if (*n_read)
				return GOT_DATA;

			sge = SSL_get_error(rc);

			if (sge == SSL_ERROR_WANT_READ)
				return NO_DATA;
			if (sge == SSL_ERROR_WANT_WRITE)
				return NO_DATA;

			return ERROR;
		}
		else if (rc == 0)
		{
			return 0;
		}
		else
		{
			wherefrom += rc;
			len -= rc;
		}
	}

	return cnt;
}

void free_sv(ssl_variables *sv)
{
	if (sv)
	{
		if (sv -> ctx)
			end_ctx(sv -> ctx);

		if (sv -> ssl_h)
			SSL_free(sv -> ssl_h);

		if (sv -> s_bio)
			BIO_free(sv -> s_bio);

		free(sv);
	}
}

ssl_variables * connect_ssl(int fd, double timeout, char **msg)
{
	int rc = -1;
	struct timeval tv;
	ssl_variables *sv = calloc(1, sizeof ssl_variables);

	*msg = NULL;

	tv.tv_sec  = (long)timeout;
	tv.tv_usec = (long)(timeout * 1000000.0) % 1000000;

	sv -> ctx = initialize_ctx(TRUE);

	if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv) == -1)
	{
		asprintf(msg, "problem setting receive timeout (%s)", strerror(errno));
		free_sv(sv);
		return NULL;
	}

	if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv) == -1)
	{
		asprintf(msg, "problem setting transmit timeout (%s)", strerror(errno));
		free_sv(sv);
		return NULL;
	}

	sv -> ssl_h = SSL_new(client_ctx);

	sv -> s_bio = BIO_new_socket(fd, BIO_NOCLOSE);
	SSL_set_bio(sv -> ssl_h, sv -> s_bio, sv -> s_bio);

	rc = SSL_connect(sv -> ssl_h);
	if (rc <= 0)
	{
		asprintf(msg, "problem starting SSL connection: %d", SSL_get_error(sv -> ssl_h, dummy));
		free_sv(sv);
		return NULL;
	}

	return sv;
}

char * get_fingerprint(SSL *ssl_h)
{
	char *string = NULL;

	unsigned char fp_digest[EVP_MAX_MD_SIZE];
	X509 *x509_data = SSL_get_peer_certificate(ssl_h);

	if (x509_data)
	{
		unsigned int fp_digest_size = sizeof fp_digest;

		memset(fp_digest, 0x00, fp_digest_size);

		if (X509_digest(x509_data, EVP_md5(), fp_digest, &fp_digest_size))
		{
			string = (char *)malloc(MD5_DIGEST_LENGTH * 3 + 1);
			if (string)
			{
				int loop, pos =0;

				for(loop=0; loop<MD5_DIGEST_LENGTH; loop++)
				{
					if (loop)
						pos += sprintf(&string[pos], ":%02x", fp_digest[loop]);
					else
						pos = sprintf(&string[pos], "%02x", fp_digest[loop]);
				}
			}
		}

		X509_free(x509_data);
	}

	return string;
}
