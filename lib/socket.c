/*
 * LibYAM -- E-Mail client library
 * Copyright (C) 2020 Victor Ananjevsky <victor@sanana.kiev.ua>
 * Copyright (C) 1999-2012 Hiroyuki Yamamoto
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <sys/time.h>
#include <sys/types.h>
#if HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <resolv.h>
#include <netdb.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <setjmp.h>
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include "socket.h"
#if USE_SSL
#include "ssl.h"
#endif

#include "utils.h"

#define BUFFSIZE	8192

#define SockDesc		gint
#define SOCKET_IS_VALID(s)	((s) >= 0)
#define INVALID_SOCKET		(-1)

typedef gint (*SockAddrFunc) (GList * addr_list, gpointer data);

typedef struct _SockConnectData SockConnectData;
typedef struct _SockLookupData SockLookupData;
typedef struct _SockAddrData SockAddrData;
typedef struct _SockSource SockSource;

struct _SockConnectData {
  gint id;
  gchar *hostname;
  gushort port;
  GList *addr_list;
  GList *cur_addr;
  SockLookupData *lookup_data;
  GIOChannel *channel;
  guint io_tag;
  gint flag;
  GThread *thread;
  SockInfo *sock;
  SockConnectFunc func;
  gpointer data;
};

struct _SockLookupData {
  gchar *hostname;
  pid_t child_pid;
  GIOChannel *channel;
  guint io_tag;
  SockAddrFunc func;
  gpointer data;
};

struct _SockAddrData {
  gint family;
  gint socktype;
  gint protocol;
  gint addr_len;
  struct sockaddr *addr;
};

struct _SockSource {
  GSource parent;
  SockInfo *sock;
};

static guint io_timeout = 60;

static GList *sock_connect_data_list = NULL;
static GList *sock_list = NULL;

static gboolean sock_prepare (GSource * source, gint * timeout);
static gboolean sock_check (GSource * source);
static gboolean sock_dispatch (GSource * source, GSourceFunc callback, gpointer user_data);

GSourceFuncs sock_watch_funcs = {
  sock_prepare,
  sock_check,
  sock_dispatch,
  NULL
};

static SockInfo *sock_find_from_fd (gint fd);

static gint sock_connect_with_timeout (gint sock, const struct sockaddr *serv_addr, gint addrlen, guint timeout_secs);

#ifndef INET6
static gint sock_info_connect_by_hostname (SockInfo * sock);
#else
static SockDesc sock_info_connect_by_getaddrinfo (SockInfo * sock);
#endif

static void sock_address_list_free (GList * addr_list);

static gboolean sock_connect_async_cb (GIOChannel * source, GIOCondition condition, gpointer data);
static gint sock_connect_async_get_address_info_cb (GList * addr_list, gpointer data);

static gint sock_connect_address_list_async (SockConnectData * conn_data);

static gboolean sock_get_address_info_async_cb (GIOChannel * source, GIOCondition condition, gpointer data);
static SockLookupData *sock_get_address_info_async
  (const gchar * hostname, gushort port, SockAddrFunc func, gpointer data);
static gint sock_get_address_info_async_cancel (SockLookupData * lookup_data);

gint
sock_set_io_timeout (guint sec)
{
  io_timeout = sec;
  return 0;
}

gint
fd_connect_inet (gushort port)
{
  SockDesc sock;
  struct sockaddr_in addr;

  sock = socket (AF_INET, SOCK_STREAM, 0);
  if (!SOCKET_IS_VALID (sock))
    {
      perror ("fd_connect_inet(): socket");
      return -1;
    }

  memset (&addr, 0, sizeof (addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons (port);
  addr.sin_addr.s_addr = htonl (INADDR_LOOPBACK);

  if (connect (sock, (struct sockaddr *) &addr, sizeof (addr)) < 0)
    {
      fd_close (sock);
      return -1;
    }

  return sock;
}

gint
fd_open_inet (gushort port)
{
  SockDesc sock;
  struct sockaddr_in addr;
  gint val;

  sock = socket (AF_INET, SOCK_STREAM, 0);
  if (!SOCKET_IS_VALID (sock))
    {
      perror ("fd_open_inet(): socket");
      return -1;
    }

  val = 1;
  if (setsockopt (sock, SOL_SOCKET, SO_REUSEADDR, (char *) &val, sizeof (val)) < 0)
    {
      perror ("setsockopt");
      fd_close (sock);
      return -1;
    }

  memset (&addr, 0, sizeof (addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons (port);
  addr.sin_addr.s_addr = htonl (INADDR_LOOPBACK);

  if (bind (sock, (struct sockaddr *) &addr, sizeof (addr)) < 0)
    {
      perror ("bind");
      fd_close (sock);
      return -1;
    }

  if (listen (sock, 1) < 0)
    {
      perror ("listen");
      fd_close (sock);
      return -1;
    }

  return sock;
}

gint
fd_connect_unix (const gchar * path)
{
  gint sock;
  struct sockaddr_un addr;

  sock = socket (PF_UNIX, SOCK_STREAM, 0);
  if (sock < 0)
    {
      perror ("fd_connect_unix(): socket");
      return -1;
    }

  memset (&addr, 0, sizeof (addr));
  addr.sun_family = AF_UNIX;
  strncpy (addr.sun_path, path, sizeof (addr.sun_path) - 1);

  if (connect (sock, (struct sockaddr *) &addr, sizeof (addr)) < 0)
    {
      fd_close (sock);
      return -1;
    }

  return sock;
}

gint
fd_open_unix (const gchar * path)
{
  gint sock;
  struct sockaddr_un addr;
  gint val;

  sock = socket (PF_UNIX, SOCK_STREAM, 0);

  if (sock < 0)
    {
      perror ("sock_open_unix(): socket");
      return -1;
    }

  val = 1;
  if (setsockopt (sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof (val)) < 0)
    {
      perror ("setsockopt");
      fd_close (sock);
      return -1;
    }

  memset (&addr, 0, sizeof (addr));
  addr.sun_family = AF_UNIX;
  strncpy (addr.sun_path, path, sizeof (addr.sun_path) - 1);

  if (bind (sock, (struct sockaddr *) &addr, sizeof (addr)) < 0)
    {
      perror ("bind");
      fd_close (sock);
      return -1;
    }

  if (listen (sock, 1) < 0)
    {
      perror ("listen");
      fd_close (sock);
      return -1;
    }

  return sock;
}

gint
fd_accept (gint sock)
{
  struct sockaddr_in caddr;
  guint caddr_len;

  caddr_len = sizeof (caddr);
  return accept (sock, (struct sockaddr *) &caddr, &caddr_len);
}


SockInfo *
sock_new (const gchar * hostname, gushort port)
{
  SockInfo *sockinfo;

  sockinfo = g_new0 (SockInfo, 1);
  sockinfo->sock = INVALID_SOCKET;
  sockinfo->sock_ch = NULL;
  sockinfo->hostname = g_strdup (hostname);
  sockinfo->port = port;
  sockinfo->state = CONN_READY;
  sockinfo->flags = 0;
  sockinfo->data = NULL;

  return sockinfo;
}

static SockInfo *
sock_find_from_fd (gint fd)
{
  GList *cur;

  for (cur = sock_list; cur != NULL; cur = cur->next)
    {
      if (((SockInfo *) cur->data)->sock == fd)
        return (SockInfo *) cur->data;
    }

  return NULL;
}

static gint
set_nonblocking_mode (gint fd, gboolean nonblock)
{
  gint flags;

  flags = fcntl (fd, F_GETFL, 0);
  if (flags < 0)
    {
      perror ("fcntl");
      return -1;
    }

  if (nonblock)
    flags |= O_NONBLOCK;
  else
    flags &= ~O_NONBLOCK;

  return fcntl (fd, F_SETFL, flags);
}

gint
sock_set_nonblocking_mode (SockInfo * sock, gboolean nonblock)
{
  gint ret;

  g_return_val_if_fail (sock != NULL, -1);

  ret = set_nonblocking_mode (sock->sock, nonblock);
  if (ret == 0)
    {
      if (nonblock)
        {
          SOCK_SET_FLAGS (sock->flags, YAM_SOCK_NONBLOCK);
        }
      else
        {
          SOCK_UNSET_FLAGS (sock->flags, YAM_SOCK_NONBLOCK);
        }
    }

  return ret;
}

static gboolean
is_nonblocking_mode (gint fd)
{
  gint flags;

  flags = fcntl (fd, F_GETFL, 0);
  if (flags < 0)
    {
      perror ("fcntl");
      return FALSE;
    }

  return ((flags & O_NONBLOCK) != 0);
}

gboolean
sock_is_nonblocking_mode (SockInfo * sock)
{
  g_return_val_if_fail (sock != NULL, FALSE);

  return is_nonblocking_mode (sock->sock);
}

static gboolean
sock_prepare (GSource * source, gint * timeout)
{
  *timeout = 1;
  return FALSE;
}

static gboolean
sock_check (GSource * source)
{
  SockInfo *sock = ((SockSource *) source)->sock;
  struct timeval timeout = { 0, 0 };
  fd_set fds;
  GIOCondition condition = sock->condition;

#if USE_SSL
  if (sock->ssl)
    {
      if (condition & G_IO_IN)
        {
          if (SSL_pending (sock->ssl) > 0)
            return TRUE;
          if (SSL_want_write (sock->ssl))
            condition |= G_IO_OUT;
        }

      if (condition & G_IO_OUT)
        {
          if (SSL_want_read (sock->ssl))
            condition |= G_IO_IN;
        }
    }
#endif

  FD_ZERO (&fds);
  FD_SET (sock->sock, &fds);

  select (sock->sock + 1, (condition & G_IO_IN) ? &fds : NULL, (condition & G_IO_OUT) ? &fds : NULL, NULL, &timeout);

  return FD_ISSET (sock->sock, &fds) != 0;
}

static gboolean
sock_dispatch (GSource * source, GSourceFunc callback, gpointer user_data)
{
  SockInfo *sock = ((SockSource *) source)->sock;

  return sock->callback (sock, sock->condition, sock->data);
}

static gboolean
sock_watch_cb (GIOChannel * source, GIOCondition condition, gpointer data)
{
  SockInfo *sock = (SockInfo *) data;

  if ((condition & sock->condition) == 0)
    return TRUE;

  return sock->callback (sock, sock->condition, sock->data);
}

guint
sock_add_watch (SockInfo * sock, GIOCondition condition, SockFunc func, gpointer data)
{
  sock->callback = func;
  sock->condition = condition;
  sock->data = data;

#if USE_SSL
  if (sock->ssl)
    {
      GSource *source;

      source = g_source_new (&sock_watch_funcs, sizeof (SockSource));
      ((SockSource *) source)->sock = sock;
      g_source_set_priority (source, G_PRIORITY_DEFAULT);
      g_source_set_can_recurse (source, FALSE);
      return g_source_attach (source, NULL);
    }
#endif

  return g_io_add_watch (sock->sock_ch, condition, sock_watch_cb, sock);
}

guint
sock_add_watch_poll (SockInfo * sock, GIOCondition condition, SockFunc func, gpointer data)
{
  GSource *source;

  sock->callback = func;
  sock->condition = condition;
  sock->data = data;

  source = g_source_new (&sock_watch_funcs, sizeof (SockSource));
  ((SockSource *) source)->sock = sock;
  g_source_set_priority (source, G_PRIORITY_DEFAULT);
  g_source_set_can_recurse (source, FALSE);

  return g_source_attach (source, NULL);
}

static gint
fd_check_io (gint fd, GIOCondition cond)
{
  struct timeval timeout;
  fd_set fds;
  SockInfo *sock;

  sock = sock_find_from_fd (fd);
  if (sock && !SOCK_IS_CHECK_IO (sock->flags))
    return 0;

  timeout.tv_sec = io_timeout;
  timeout.tv_usec = 0;

  FD_ZERO (&fds);
  FD_SET (fd, &fds);

  if (cond == G_IO_IN)
    select (fd + 1, &fds, NULL, NULL, io_timeout > 0 ? &timeout : NULL);
  else
    select (fd + 1, NULL, &fds, NULL, io_timeout > 0 ? &timeout : NULL);

  if (FD_ISSET (fd, &fds))
    return 0;
  else
    {
      g_warning ("Socket IO timeout\n");
      return -1;
    }
}

static gint
sock_connect_with_timeout (gint sock, const struct sockaddr *serv_addr, gint addrlen, guint timeout_secs)
{
  gint ret;

  set_nonblocking_mode (sock, TRUE);

  ret = connect (sock, serv_addr, addrlen);

  if (ret < 0)
    {
      if (EINPROGRESS == errno)
        {
          fd_set fds;
          struct timeval tv;

          tv.tv_sec = timeout_secs;
          tv.tv_usec = 0;
          FD_ZERO (&fds);
          FD_SET (sock, &fds);
          do
            ret = select (sock + 1, NULL, &fds, NULL, &tv);
          while (ret < 0 && EINTR == errno);
          if (ret < 0)
            {
              perror ("sock_connect_with_timeout: select");
              return -1;
            }
          else if (ret == 0)
            {
              debug_print ("sock_connect_with_timeout: timeout\n");
              errno = ETIMEDOUT;
              return -1;
            }
          else
            {
              gint val;
              guint len;

              if (FD_ISSET (sock, &fds))
                {
                  ret = 0;
                }
              else
                {
                  debug_print ("sock_connect_with_timeout: fd not set\n");
                  return -1;
                }

              len = sizeof (val);
              if (getsockopt (sock, SOL_SOCKET, SO_ERROR, &val, &len) < 0)
                {
                  perror ("sock_connect_with_timeout: getsockopt");
                  return -1;
                }
              if (val != 0)
                {
                  debug_print ("sock_connect_with_timeout: getsockopt(SOL_SOCKET, SO_ERROR) returned error: %s\n",
                               g_strerror (val));
                  return -1;
                }
            }
        }
      else
        {
          perror ("sock_connect_with_timeout: connect");
          return -1;
        }
    }

  set_nonblocking_mode (sock, FALSE);

  return ret;
}

static void
resolver_init (void)
{
  static time_t resolv_conf_mtime = 0;
  GStatBuf s;

  if (g_stat ("/etc/resolv.conf", &s) == 0 && s.st_mtime != resolv_conf_mtime)
    {
      debug_print ("Reloading /etc/resolv.conf\n");
      resolv_conf_mtime = s.st_mtime;
      res_init ();
    }
}

struct hostent *
my_gethostbyname (const gchar * hostname)
{
  struct hostent *hp;

  if ((hp = gethostbyname (hostname)) == NULL)
    {
      fprintf (stderr, "%s: unknown host.\n", hostname);
      errno = 0;
      return NULL;
    }

  return hp;
}

#if !defined(INET6)
static gint
my_inet_aton (const gchar * hostname, struct in_addr *inp)
{
#if HAVE_INET_ATON
  return inet_aton (hostname, inp);
#else
#if HAVE_INET_ADDR
  guint32 inaddr;

  inaddr = inet_addr (hostname);
  if (inaddr != -1)
    {
      memcpy (inp, &inaddr, sizeof (inaddr));
      return 1;
    }
  else
    return 0;
#else
  return 0;
#endif
#endif /* HAVE_INET_ATON */
}
#endif /* !defined(INET6) */

#ifndef INET6
static gint
sock_info_connect_by_hostname (SockInfo * sock)
{
  struct hostent *hp;
  struct sockaddr_in ad;
  gint ret;

  g_return_val_if_fail (sock != NULL, -1);
  g_return_val_if_fail (sock->hostname != NULL && sock->port > 0, -1);

  resolver_init ();

  memset (&ad, 0, sizeof (ad));
  ad.sin_family = AF_INET;
  ad.sin_port = htons (sock->port);

  if (!my_inet_aton (sock->hostname, &ad.sin_addr))
    {
      if ((hp = my_gethostbyname (sock->hostname)) == NULL)
        {
          fprintf (stderr, "%s: unknown host.\n", sock->hostname);
          errno = 0;
          sock->state = CONN_LOOKUPFAILED;
          return -1;
        }

      if (hp->h_length != 4 && hp->h_length != 8)
        {
          fprintf (stderr, "illegal address length received for host %s\n", sock->hostname);
          errno = 0;
          sock->state = CONN_LOOKUPFAILED;
          return -1;
        }

      memcpy (&ad.sin_addr, hp->h_addr, hp->h_length);
    }

  sock->state = CONN_LOOKUPSUCCESS;

  ret = sock_connect_with_timeout (sock->sock, (struct sockaddr *) &ad, sizeof (ad), io_timeout);
  if (ret < 0)
    sock->state = CONN_FAILED;
  else
    sock->state = CONN_ESTABLISHED;

  return ret;
}

#else /* INET6 */

static SockDesc
sock_info_connect_by_getaddrinfo (SockInfo * sockinfo)
{
  SockDesc sock = INVALID_SOCKET;
  gint gai_error;
  struct addrinfo hints, *res, *ai;
  gchar port_str[6];

  g_return_val_if_fail (sockinfo != NULL, INVALID_SOCKET);
  g_return_val_if_fail (sockinfo->hostname != NULL && sockinfo->port > 0, INVALID_SOCKET);

  resolver_init ();

  memset (&hints, 0, sizeof (hints));
  /* hints.ai_flags = AI_CANONNAME; */
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  /* convert port from integer to string. */
  g_snprintf (port_str, sizeof (port_str), "%d", sockinfo->port);

  if ((gai_error = getaddrinfo (sockinfo->hostname, port_str, &hints, &res)) != 0)
    {
      fprintf (stderr, "getaddrinfo for %s:%s failed: %s\n", sockinfo->hostname, port_str, gai_strerror (gai_error));
      debug_print ("getaddrinfo failed\n");
      sockinfo->state = CONN_LOOKUPFAILED;
      return INVALID_SOCKET;
    }

  for (ai = res; ai != NULL; ai = ai->ai_next)
    {
      sock = socket (ai->ai_family, ai->ai_socktype, ai->ai_protocol);
      if (!SOCKET_IS_VALID (sock))
        continue;

      if (sock_connect_with_timeout (sock, ai->ai_addr, ai->ai_addrlen, io_timeout) == 0)
        break;

      fd_close (sock);
    }

  if (res != NULL)
    freeaddrinfo (res);

  if (ai == NULL)
    {
      sockinfo->state = CONN_FAILED;
      return INVALID_SOCKET;
    }

  sockinfo->state = CONN_ESTABLISHED;
  return sock;
}
#endif /* !INET6 */

SockInfo *
sock_connect (const gchar * hostname, gushort port)
{
  SockInfo *sockinfo;

  sockinfo = sock_new (hostname, port);
  if (sock_info_connect (sockinfo) < 0)
    {
      sock_close (sockinfo);
      return NULL;
    }

  return sockinfo;
}

gint
sock_info_connect (SockInfo * sockinfo)
{
  SockDesc sock;
#ifndef INET6
  gint ret;
#endif

  g_return_val_if_fail (sockinfo != NULL, -1);
  g_return_val_if_fail (sockinfo->hostname != NULL && sockinfo->port > 0, -1);

#ifdef INET6
  sock = sock_info_connect_by_getaddrinfo (sockinfo);
  if (!SOCKET_IS_VALID (sock))
    {
      return -1;
    }
#else
  sock = socket (AF_INET, SOCK_STREAM, 0);
  if (!SOCKET_IS_VALID (sock))
    {
      perror ("socket");
      sockinfo->state = CONN_FAILED;
      return -1;
    }

  sockinfo->sock = sock;
  if ((ret = sock_info_connect_by_hostname (sockinfo)) < 0)
    {
      if (errno != 0)
        perror ("connect");
      fd_close (sock);
      sockinfo->sock = INVALID_SOCKET;
      return ret;
    }
#endif /* INET6 */

  sockinfo->sock = sock;
  sockinfo->sock_ch = g_io_channel_unix_new (sock);
  sockinfo->flags = YAM_SOCK_CHECK_IO;

  g_io_channel_set_encoding (sockinfo->sock_ch, NULL, NULL);

  sock_list = g_list_prepend (sock_list, sockinfo);

  g_usleep (100000);

  return 0;
}

static void
sock_address_list_free (GList * addr_list)
{
  GList *cur;

  for (cur = addr_list; cur != NULL; cur = cur->next)
    {
      SockAddrData *addr_data = (SockAddrData *) cur->data;
      g_free (addr_data->addr);
      g_free (addr_data);
    }

  g_list_free (addr_list);
}

/* asynchronous TCP connection */
static gboolean
sock_connect_async_cb (GIOChannel * source, GIOCondition condition, gpointer data)
{
  SockConnectData *conn_data = (SockConnectData *) data;
  gint fd;
  gint val;
  guint len;
  SockInfo *sockinfo;

  fd = g_io_channel_unix_get_fd (source);

  conn_data->io_tag = 0;
  conn_data->channel = NULL;
  g_io_channel_unref (source);

  if (condition & (G_IO_ERR | G_IO_HUP | G_IO_NVAL))
    {
      debug_print ("sock_connect_async_cb: condition = %d\n", condition);
      fd_close (fd);
      sock_connect_address_list_async (conn_data);
      return FALSE;
    }

  len = sizeof (val);
  if (getsockopt (fd, SOL_SOCKET, SO_ERROR, &val, &len) < 0)
    {
      perror ("getsockopt");
      fd_close (fd);
      sock_connect_address_list_async (conn_data);
      return FALSE;
    }

  if (val != 0)
    {
      debug_print ("getsockopt(SOL_SOCKET, SO_ERROR) returned error\n");
      fd_close (fd);
      sock_connect_address_list_async (conn_data);
      return FALSE;
    }

  sockinfo = conn_data->sock;
  sockinfo->sock = fd;
  sockinfo->sock_ch = g_io_channel_unix_new (fd);
  sockinfo->state = CONN_ESTABLISHED;
  sockinfo->flags = YAM_SOCK_NONBLOCK;

  g_io_channel_set_encoding (sockinfo->sock_ch, NULL, NULL);

  sock_list = g_list_prepend (sock_list, sockinfo);

  conn_data->func (sockinfo, conn_data->data);

  conn_data->sock = NULL;
  sock_connect_async_cancel (conn_data->id);

  return FALSE;
}

static gint
sock_connect_async_get_address_info_cb (GList * addr_list, gpointer data)
{
  SockConnectData *conn_data = (SockConnectData *) data;

  conn_data->addr_list = addr_list;
  conn_data->cur_addr = addr_list;
  conn_data->lookup_data = NULL;

  return sock_connect_address_list_async (conn_data);
}

gint
sock_connect_async (const gchar * hostname, gushort port, SockConnectFunc func, gpointer data)
{
  SockInfo *sock;
  gint ret;

  sock = sock_new (hostname, port);
  ret = sock_info_connect_async (sock, func, data);
  if (ret < 0)
    sock_close (sock);

  return ret;
}

gint
sock_info_connect_async (SockInfo * sock, SockConnectFunc func, gpointer data)
{
  static gint id = 1;
  SockConnectData *conn_data;

  g_return_val_if_fail (sock != NULL, -1);
  g_return_val_if_fail (sock->hostname != NULL && sock->port > 0, -1);

  conn_data = g_new0 (SockConnectData, 1);
  conn_data->id = id++;
  conn_data->hostname = g_strdup (sock->hostname);
  conn_data->port = sock->port;
  conn_data->addr_list = NULL;
  conn_data->cur_addr = NULL;
  conn_data->io_tag = 0;
  conn_data->sock = sock;
  conn_data->func = func;
  conn_data->data = data;

  conn_data->lookup_data = sock_get_address_info_async
    (sock->hostname, sock->port, sock_connect_async_get_address_info_cb, conn_data);

  if (conn_data->lookup_data == NULL)
    {
      g_free (conn_data->hostname);
      g_free (conn_data);
      return -1;
    }

  sock_connect_data_list = g_list_append (sock_connect_data_list, conn_data);

  return conn_data->id;
}

gint
sock_connect_async_cancel (gint id)
{
  SockConnectData *conn_data = NULL;
  GList *cur;

  for (cur = sock_connect_data_list; cur != NULL; cur = cur->next)
    {
      if (((SockConnectData *) cur->data)->id == id)
        {
          conn_data = (SockConnectData *) cur->data;
          break;
        }
    }

  if (conn_data)
    {
      sock_connect_data_list = g_list_remove (sock_connect_data_list, conn_data);

      if (conn_data->lookup_data)
        sock_get_address_info_async_cancel (conn_data->lookup_data);

      if (conn_data->io_tag > 0)
        g_source_remove (conn_data->io_tag);
      if (conn_data->channel)
        {
          g_io_channel_shutdown (conn_data->channel, FALSE, NULL);
          g_io_channel_unref (conn_data->channel);
        }
      if (conn_data->sock)
        sock_close (conn_data->sock);

      sock_address_list_free (conn_data->addr_list);
      g_free (conn_data->hostname);
      g_free (conn_data);
    }
  else
    {
      g_warning ("sock_connect_async_cancel: id %d not found.\n", id);
      return -1;
    }

  return 0;
}

static gint
sock_connect_address_list_async (SockConnectData * conn_data)
{
  SockAddrData *addr_data;
  gint sock = -1;

  if (conn_data->addr_list == NULL)
    {
      g_warning ("sock_connect_address_list_async: DNS lookup for %s failed", conn_data->hostname);
      conn_data->sock->state = CONN_LOOKUPFAILED;
      conn_data->func (conn_data->sock, conn_data->data);
      sock_connect_async_cancel (conn_data->id);
      return -1;
    }

  for (; conn_data->cur_addr != NULL; conn_data->cur_addr = conn_data->cur_addr->next)
    {
      addr_data = (SockAddrData *) conn_data->cur_addr->data;

      if ((sock = socket (addr_data->family, addr_data->socktype, addr_data->protocol)) < 0)
        {
          perror ("socket");
          continue;
        }

      set_nonblocking_mode (sock, TRUE);

      if (connect (sock, addr_data->addr, addr_data->addr_len) < 0)
        {
          if (EINPROGRESS == errno)
            break;
          else
            {
              perror ("connect");
              fd_close (sock);
            }
        }
      else
        break;
    }

  if (conn_data->cur_addr == NULL)
    {
      g_warning ("sock_connect_address_list_async: connection to %s:%d failed", conn_data->hostname, conn_data->port);
      conn_data->sock->state = CONN_FAILED;
      conn_data->func (conn_data->sock, conn_data->data);
      sock_connect_async_cancel (conn_data->id);
      return -1;
    }

  debug_print ("sock_connect_address_list_async: waiting for connect\n");

  conn_data->cur_addr = conn_data->cur_addr->next;

  conn_data->channel = g_io_channel_unix_new (sock);
  g_io_channel_set_encoding (conn_data->channel, NULL, NULL);
  conn_data->io_tag = g_io_add_watch
    (conn_data->channel, G_IO_OUT | G_IO_ERR | G_IO_HUP | G_IO_NVAL, sock_connect_async_cb, conn_data);

  return 0;
}

static gint
sock_kill_process (pid_t pid)
{
  pid_t ret = (pid_t) - 1;

  kill (pid, SIGKILL);

  while (ret == (pid_t) - 1)
    {
      if ((ret = waitpid (pid, NULL, 0)) != pid)
        {
          if (ret == (pid_t) - 1 && errno != EINTR)
            {
              perror ("sock_kill_process(): waitpid");
              break;
            }
        }
    }

  return (gint) pid;
}

/* asynchronous DNS lookup */

static gboolean
sock_get_address_info_async_cb (GIOChannel * source, GIOCondition condition, gpointer data)
{
  SockLookupData *lookup_data = (SockLookupData *) data;
  GList *addr_list = NULL;
  SockAddrData *addr_data;
  gsize bytes_read;
  gint ai_member[4];
  struct sockaddr *addr;

  for (;;)
    {
      if (g_io_channel_read_chars (source, (gchar *) ai_member, sizeof (ai_member), &bytes_read, NULL) != G_IO_STATUS_NORMAL)
        {
          g_warning ("sock_get_address_info_async_cb: address length read error\n");
          break;
        }

      if (bytes_read == 0 || bytes_read != sizeof (ai_member))
        break;

      if (ai_member[0] == AF_UNSPEC)
        {
          g_warning ("DNS lookup failed");
          break;
        }

      addr = g_malloc (ai_member[3]);
      if (g_io_channel_read_chars (source, (gchar *) addr, ai_member[3], &bytes_read, NULL) != G_IO_STATUS_NORMAL)
        {
          g_warning ("sock_get_address_info_async_cb: address data read error\n");
          g_free (addr);
          break;
        }

      if (bytes_read != ai_member[3])
        {
          g_warning ("sock_get_address_info_async_cb: incomplete address data\n");
          g_free (addr);
          break;
        }

      addr_data = g_new0 (SockAddrData, 1);
      addr_data->family = ai_member[0];
      addr_data->socktype = ai_member[1];
      addr_data->protocol = ai_member[2];
      addr_data->addr_len = ai_member[3];
      addr_data->addr = addr;

      addr_list = g_list_append (addr_list, addr_data);
    }

  g_io_channel_shutdown (source, FALSE, NULL);
  g_io_channel_unref (source);

  sock_kill_process (lookup_data->child_pid);

  lookup_data->func (addr_list, lookup_data->data);

  g_free (lookup_data->hostname);
  g_free (lookup_data);

  return FALSE;
}

static SockLookupData *
sock_get_address_info_async (const gchar * hostname, gushort port, SockAddrFunc func, gpointer data)
{
  SockLookupData *lookup_data = NULL;
  gint pipe_fds[2];
  pid_t pid;

  resolver_init ();

  if (pipe (pipe_fds) < 0)
    {
      perror ("pipe");
      func (NULL, data);
      return NULL;
    }

  if ((pid = fork ()) < 0)
    {
      perror ("fork");
      func (NULL, data);
      return NULL;
    }

  /* child process */
  if (pid == 0)
    {
#ifdef INET6
      gint gai_err;
      struct addrinfo hints, *res, *ai;
      gchar port_str[6];
#else /* !INET6 */
      struct hostent *hp;
      gchar **addr_list_p;
      struct sockaddr_in ad;
#endif /* INET6 */
      gint ai_member[4] = { AF_UNSPEC, 0, 0, 0 };

      close (pipe_fds[0]);

#ifdef INET6
      memset (&hints, 0, sizeof (hints));
      /* hints.ai_flags = AI_CANONNAME; */
      hints.ai_family = AF_UNSPEC;
      hints.ai_socktype = SOCK_STREAM;
      hints.ai_protocol = IPPROTO_TCP;

      g_snprintf (port_str, sizeof (port_str), "%d", port);

      gai_err = getaddrinfo (hostname, port_str, &hints, &res);
      if (gai_err != 0)
        {
          g_warning ("getaddrinfo for %s:%s failed: %s", hostname, port_str, gai_strerror (gai_err));
          fd_write_all (pipe_fds[1], (gchar *) ai_member, sizeof (ai_member));
          close (pipe_fds[1]);
          _exit (1);
        }

      for (ai = res; ai != NULL; ai = ai->ai_next)
        {
          ai_member[0] = ai->ai_family;
          ai_member[1] = ai->ai_socktype;
          ai_member[2] = ai->ai_protocol;
          ai_member[3] = ai->ai_addrlen;

          fd_write_all (pipe_fds[1], (gchar *) ai_member, sizeof (ai_member));
          fd_write_all (pipe_fds[1], (gchar *) ai->ai_addr, ai->ai_addrlen);
        }

      if (res != NULL)
        freeaddrinfo (res);
#else /* !INET6 */
      hp = my_gethostbyname (hostname);
      if (hp == NULL || hp->h_addrtype != AF_INET)
        {
          fd_write_all (pipe_fds[1], (gchar *) ai_member, sizeof (ai_member));
          close (pipe_fds[1]);
          _exit (1);
        }

      ai_member[0] = AF_INET;
      ai_member[1] = SOCK_STREAM;
      ai_member[2] = IPPROTO_TCP;
      ai_member[3] = sizeof (ad);

      memset (&ad, 0, sizeof (ad));
      ad.sin_family = AF_INET;
      ad.sin_port = htons (port);

      for (addr_list_p = hp->h_addr_list; *addr_list_p != NULL; addr_list_p++)
        {
          memcpy (&ad.sin_addr, *addr_list_p, hp->h_length);
          fd_write_all (pipe_fds[1], (gchar *) ai_member, sizeof (ai_member));
          fd_write_all (pipe_fds[1], (gchar *) & ad, sizeof (ad));
        }
#endif /* INET6 */

      close (pipe_fds[1]);

      _exit (0);
    }
  else
    {
      close (pipe_fds[1]);

      lookup_data = g_new0 (SockLookupData, 1);
      lookup_data->hostname = g_strdup (hostname);
      lookup_data->child_pid = pid;
      lookup_data->func = func;
      lookup_data->data = data;

      lookup_data->channel = g_io_channel_unix_new (pipe_fds[0]);
      g_io_channel_set_encoding (lookup_data->channel, NULL, NULL);
      lookup_data->io_tag = g_io_add_watch (lookup_data->channel, G_IO_IN, sock_get_address_info_async_cb, lookup_data);
    }

  return lookup_data;
}

static gint
sock_get_address_info_async_cancel (SockLookupData * lookup_data)
{
  if (lookup_data->io_tag > 0)
    g_source_remove (lookup_data->io_tag);
  if (lookup_data->channel)
    {
      g_io_channel_shutdown (lookup_data->channel, FALSE, NULL);
      g_io_channel_unref (lookup_data->channel);
    }

  if (lookup_data->child_pid > 0)
    sock_kill_process (lookup_data->child_pid);

  g_free (lookup_data->hostname);
  g_free (lookup_data);

  return 0;
}

static gpointer
sock_connect_async_func (gpointer data)
{
  SockConnectData *conn_data = (SockConnectData *) data;
  gint ret;

  ret = sock_info_connect (conn_data->sock);

  if (ret == 0)
    {
      debug_print ("sock_connect_async_func: connected\n");
    }
  else
    {
      if (conn_data->sock->state == CONN_LOOKUPFAILED)
        debug_print ("sock_connect_async_func: DNS lookup failed\n");
      else
        debug_print ("sock_connect_async_func: connection failed\n");
    }

  g_atomic_int_set (&conn_data->flag, 1);
  g_main_context_wakeup (NULL);

  debug_print ("sock_connect_async_func: exit\n");
  return GINT_TO_POINTER (ret);
}

gint
sock_connect_async_thread (const gchar * hostname, gushort port)
{
  SockInfo *sock;
  gint ret;

  sock = sock_new (hostname, port);
  ret = sock_info_connect_async_thread (sock);
  if (ret < 0)
    sock_close (sock);

  return ret;
}

gint
sock_info_connect_async_thread (SockInfo * sock)
{
  static gint id = 1;
  SockConnectData *data;

  g_return_val_if_fail (sock != NULL, -1);
  g_return_val_if_fail (sock->hostname != NULL && sock->port > 0, -1);

  data = g_new0 (SockConnectData, 1);
  data->id = id++;
  data->hostname = g_strdup (sock->hostname);
  data->port = sock->port;
  data->flag = 0;
  data->sock = sock;

  data->thread = g_thread_new ("sock_info", sock_connect_async_func, data);
  if (!data->thread)
    {
      g_free (data->hostname);
      g_free (data);
      return -1;
    }

  sock_connect_data_list = g_list_append (sock_connect_data_list, data);

  return data->id;
}

gint
sock_connect_async_thread_wait (gint id, SockInfo ** sock)
{
  gint ret;

  *sock = NULL;
  ret = sock_info_connect_async_thread_wait (id, sock);
  if (ret < 0)
    {
      if (*sock)
        {
          sock_close (*sock);
          *sock = NULL;
        }
    }

  return ret;
}

gint
sock_info_connect_async_thread_wait (gint id, SockInfo ** sock)
{
  SockConnectData *conn_data = NULL;
  GList *cur;
  gint ret;

  for (cur = sock_connect_data_list; cur != NULL; cur = cur->next)
    {
      if (((SockConnectData *) cur->data)->id == id)
        {
          conn_data = (SockConnectData *) cur->data;
          break;
        }
    }

  if (!conn_data)
    {
      g_warning ("sock_info_connect_async_thread_wait: id %d not found.", id);
      return -1;
    }

  debug_print ("sock_connect_async_thread_wait: waiting thread\n");
  while (g_atomic_int_get (&conn_data->flag) == 0)
    event_loop_iterate ();

  ret = GPOINTER_TO_INT (g_thread_join (conn_data->thread));
  debug_print ("sock_info_connect_async_thread_wait: thread exited with status %d\n", ret);

  if (sock)
    *sock = conn_data->sock;

  sock_connect_data_list = g_list_remove (sock_connect_data_list, conn_data);
  g_free (conn_data->hostname);
  g_free (conn_data);

  return ret;
}

gint
sock_printf (SockInfo * sock, const gchar * format, ...)
{
  va_list args;
  gchar buf[BUFFSIZE];

  va_start (args, format);
  g_vsnprintf (buf, sizeof (buf), format, args);
  va_end (args);

  return sock_write_all (sock, buf, strlen (buf));
}

gint
sock_read (SockInfo * sock, gchar * buf, gint len)
{
  g_return_val_if_fail (sock != NULL, -1);

#if USE_SSL
  if (sock->ssl)
    return ssl_read (sock->ssl, buf, len);
#endif
  return fd_read (sock->sock, buf, len);
}

gint
fd_read (gint fd, gchar * buf, gint len)
{
  if (fd_check_io (fd, G_IO_IN) < 0)
    return -1;

  return read (fd, buf, len);
}

#if USE_SSL
gint
ssl_read (SSL * ssl, gchar * buf, gint len)
{
  gint err, ret;

  errno = 0;

  if (SSL_pending (ssl) == 0)
    {
      if (fd_check_io (SSL_get_rfd (ssl), G_IO_IN) < 0)
        return -1;
    }

  ret = SSL_read (ssl, buf, len);

  switch ((err = SSL_get_error (ssl, ret)))
    {
    case SSL_ERROR_NONE:
      return ret;
    case SSL_ERROR_WANT_READ:
    case SSL_ERROR_WANT_WRITE:
      errno = EAGAIN;
      return -1;
    case SSL_ERROR_ZERO_RETURN:
      return 0;
    default:
      g_warning ("SSL_read() returned error %d, ret = %d\n", err, ret);
      if (ret == 0)
        return 0;
      return -1;
    }
}
#endif

gint
sock_write (SockInfo * sock, const gchar * buf, gint len)
{
  g_return_val_if_fail (sock != NULL, -1);

#if USE_SSL
  if (sock->ssl)
    return ssl_write (sock->ssl, buf, len);
#endif
  return fd_write (sock->sock, buf, len);
}

gint
fd_write (gint fd, const gchar * buf, gint len)
{
  if (fd_check_io (fd, G_IO_OUT) < 0)
    return -1;

  return write (fd, buf, len);
}

#if USE_SSL
gint
ssl_write (SSL * ssl, const gchar * buf, gint len)
{
  gint ret;

  ret = SSL_write (ssl, buf, len);

  switch (SSL_get_error (ssl, ret))
    {
    case SSL_ERROR_NONE:
      return ret;
    case SSL_ERROR_WANT_READ:
    case SSL_ERROR_WANT_WRITE:
      errno = EAGAIN;
      return -1;
    default:
      return -1;
    }
}
#endif

gint
sock_write_all (SockInfo * sock, const gchar * buf, gint len)
{
  g_return_val_if_fail (sock != NULL, -1);

#if USE_SSL
  if (sock->ssl)
    return ssl_write_all (sock->ssl, buf, len);
#endif
  return fd_write_all (sock->sock, buf, len);
}

gint
fd_write_all (gint fd, const gchar * buf, gint len)
{
  gint n, wrlen = 0;

  while (len)
    {
      n = fd_write (fd, buf, len);
      if (n <= 0)
        return -1;
      len -= n;
      wrlen += n;
      buf += n;
    }

  return wrlen;
}

#if USE_SSL
gint
ssl_write_all (SSL * ssl, const gchar * buf, gint len)
{
  gint n, wrlen = 0;

  while (len)
    {
      n = ssl_write (ssl, buf, len);
      if (n <= 0)
        return -1;
      len -= n;
      wrlen += n;
      buf += n;
    }

  return wrlen;
}
#endif

gint
fd_recv (gint fd, gchar * buf, gint len, gint flags)
{
  if (fd_check_io (fd, G_IO_IN) < 0)
    return -1;

  return recv (fd, buf, len, flags);
}

gint
fd_gets (gint fd, gchar * buf, gint len)
{
  gchar *newline, *bp = buf;
  gint n;

  if (--len < 1)
    return -1;
  do
    {
      if ((n = fd_recv (fd, bp, len, MSG_PEEK)) <= 0)
        return -1;
      if ((newline = memchr (bp, '\n', n)) != NULL)
        n = newline - bp + 1;
      if ((n = fd_read (fd, bp, n)) < 0)
        return -1;
      bp += n;
      len -= n;
    }
  while (!newline && len);

  *bp = '\0';
  return bp - buf;
}

#if USE_SSL
gint
ssl_gets (SSL * ssl, gchar * buf, gint len)
{
  gchar *newline, *bp = buf;
  gint n;

  if (--len < 1)
    return -1;
  do
    {
      if ((n = ssl_peek (ssl, bp, len)) <= 0)
        return -1;
      if ((newline = memchr (bp, '\n', n)) != NULL)
        n = newline - bp + 1;
      if ((n = ssl_read (ssl, bp, n)) < 0)
        return -1;
      bp += n;
      len -= n;
    }
  while (!newline && len);

  *bp = '\0';
  return bp - buf;
}
#endif

gint
sock_gets (SockInfo * sock, gchar * buf, gint len)
{
  g_return_val_if_fail (sock != NULL, -1);

#if USE_SSL
  if (sock->ssl)
    return ssl_gets (sock->ssl, buf, len);
#endif
  return fd_gets (sock->sock, buf, len);
}

gint
fd_getline (gint fd, gchar ** line)
{
  gchar buf[BUFFSIZE];
  gchar *str = NULL;
  gint len;
  gulong size = 0;
  gulong cur_offset = 0;

  while ((len = fd_gets (fd, buf, sizeof (buf))) > 0)
    {
      size += len;
      str = g_realloc (str, size + 1);
      memcpy (str + cur_offset, buf, len + 1);
      cur_offset += len;
      if (buf[len - 1] == '\n')
        break;
    }

  *line = str;

  if (!str)
    return -1;
  else
    return (gint) size;
}

#if USE_SSL
gint
ssl_getline (SSL * ssl, gchar ** line)
{
  gchar buf[BUFFSIZE];
  gchar *str = NULL;
  gint len;
  gulong size = 0;
  gulong cur_offset = 0;

  while ((len = ssl_gets (ssl, buf, sizeof (buf))) > 0)
    {
      size += len;
      str = g_realloc (str, size + 1);
      memcpy (str + cur_offset, buf, len + 1);
      cur_offset += len;
      if (buf[len - 1] == '\n')
        break;
    }

  *line = str;

  if (!str)
    return -1;
  else
    return (gint) size;
}
#endif

gint
sock_getline (SockInfo * sock, gchar ** line)
{
  g_return_val_if_fail (sock != NULL, -1);
  g_return_val_if_fail (line != NULL, -1);

#if USE_SSL
  if (sock->ssl)
    return ssl_getline (sock->ssl, line);
#endif
  return fd_getline (sock->sock, line);
}

gint
sock_puts (SockInfo * sock, const gchar * buf)
{
  gint ret;

  if ((ret = sock_write_all (sock, buf, strlen (buf))) < 0)
    return ret;
  return sock_write_all (sock, "\r\n", 2);
}

/* peek at the socket data without actually reading it */
#if USE_SSL
gint
ssl_peek (SSL * ssl, gchar * buf, gint len)
{
  gint err, ret;

  if (SSL_pending (ssl) == 0)
    {
      if (fd_check_io (SSL_get_rfd (ssl), G_IO_IN) < 0)
        return -1;
    }

  ret = SSL_peek (ssl, buf, len);

  switch ((err = SSL_get_error (ssl, ret)))
    {
    case SSL_ERROR_NONE:
      return ret;
    case SSL_ERROR_WANT_READ:
    case SSL_ERROR_WANT_WRITE:
      errno = EAGAIN;
      return -1;
    case SSL_ERROR_ZERO_RETURN:
      return 0;
    default:
      g_warning ("SSL_peek() returned error %d, ret = %d\n", err, ret);
      if (ret == 0)
        return 0;
      return -1;
    }
}
#endif

gint
sock_peek (SockInfo * sock, gchar * buf, gint len)
{
  g_return_val_if_fail (sock != NULL, -1);

#if USE_SSL
  if (sock->ssl)
    return ssl_peek (sock->ssl, buf, len);
#endif
  return fd_recv (sock->sock, buf, len, MSG_PEEK);
}

gint
sock_close (SockInfo * sock)
{
  GList *cur;

  if (!sock)
    return 0;

  debug_print ("sock_close: %s:%u (%p)\n", sock->hostname ? sock->hostname : "(none)", sock->port, sock);

#if USE_SSL
  if (sock->ssl)
    ssl_done_socket (sock);
#endif

  if (sock->sock_ch)
    {
      g_io_channel_shutdown (sock->sock_ch, FALSE, NULL);
      g_io_channel_unref (sock->sock_ch);
    }

  for (cur = sock_list; cur != NULL; cur = cur->next)
    {
      if ((SockInfo *) cur->data == sock)
        {
          sock_list = g_list_remove (sock_list, sock);
          break;
        }
    }

  g_free (sock->hostname);
  g_free (sock);

  return 0;
}

gint
fd_close (gint fd)
{
  return close (fd);
}
