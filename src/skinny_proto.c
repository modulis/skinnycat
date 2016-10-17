/**
 * skinnycat -- Skinny Cisco protocol command line utility
 * Copyright (C) 2016, Stas Kobzar <stas@modulis.ca>
 *
 * This file is part of skinnycat.
 *
 * skinnycat is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * skinnycat is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with skinnycat.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file skinny_proto.c
 * @brief Basic implementation of Skinny protocol.
 *
 * @author Stas Kobzar <stas@modulis.ca>
 */
#include "skinny_proto.h"
#include "skinnycat.h"

apr_status_t skinny_uac_run (apr_pool_t *mp,
                             skinnycat_opts *opts,
                             apr_socket_t *sock)
{
  apr_status_t rv;
  switch(opts->action) {
    case AID_REGISTER:
      rv = callflaw_register (mp, opts, sock);
      break;
    default:
      rv = APR_BADARG;
  }
  return rv;
}

apr_status_t callflaw_register (apr_pool_t *mp,
                                skinnycat_opts *opts,
                                apr_socket_t *sock)
{
  apr_status_t rv;
  apr_size_t size;
  skinny_msg_id mid;
  char *buf;

  size = create_msg_register (mp, &buf, opts, sock_local_ip (sock));
  LOG_VERB("--> Register device %s", opts->mac);
  rv = apr_socket_send (sock, buf, &size);
  LOG_DBG("Sent packet REGISTER with length %d, returned: %d", size, rv);

  size = create_msg_ipport (mp, &buf);
  LOG_VERB("--> IP port");
  rv = apr_socket_send (sock, buf, &size);
  LOG_DBG("Sent packet IPPORT with length %d, returned: %d", size, rv);

  for (;;) {
    char inbuf[SKINNY_MAX_PACK_LEN];
    char *ptr_buf;
    struct skinny_message *msg = (struct skinny_message*) apr_palloc (mp, sizeof(struct skinny_message));
    ptr_buf = inbuf;
    apr_size_t len = sizeof(inbuf);
    rv = apr_socket_recv(sock, ptr_buf, &len);
    mid = unpack_message (ptr_buf, msg);

    if (mid == MID_REGISTER_ACK) {
      LOG_VERB("<-- Register ACK");

    } else if (mid == MID_REGISTER_REJECT) {

      LOG_VERB("<-- Register Rejected");
  break;

    } else if (mid == MID_SETLAMP) {

      LOG_VERB("<-- Set Lamp %s for %s",
          lamp_mode_to_str (msg->data->setlamp.lampMode),
          btn_def_to_str (msg->data->setlamp.stimulus));

    } else if (mid == MID_CAPABILITIES_REQ) {

      LOG_VERB("<-- Capabilities request");
      size = create_msg_cap_res (mp, &buf);
      LOG_VERB("--> Capabilities response");
      rv = apr_socket_send (sock, buf, &size);
      LOG_DBG("Sent packet CAPABILITIES RES with length %d, returned: %d", size, rv);
      LOG_VERB("--> Button template request");
      size = create_msg_btn_tmpl_req (mp, &buf);
      rv = apr_socket_send (sock, buf, &size);
      LOG_DBG("Sent packet BUTTON TEMPLATE REQ with length %d, returned: %d", size, rv);

    } else if (mid == MID_INVALID) {

      LOG_ERR("Unrecognized or unsupported message : %d",
          ((struct skinny_header *)inbuf)->msg_id );
      rv = APR_ENOTIMPL;
  break;

    } else if (mid == MID_BUTTON_TMPL ) {
      LOG_VERB("<-- Device buttons template. Total buttons: %d", msg->data->btn_tmpl.btn_total);
      for (int i = 0; i < msg->data->btn_tmpl.btn_total; i++) {
        LOG_VERB("    [Button #%d] : %s", (i + 1),
            btn_def_to_str(msg->data->btn_tmpl.btn[i].definition));
      }
      rv = APR_SUCCESS;
  //break;
    }
  }

  return rv;
}

unsigned long sock_local_ip(apr_socket_t *sock)
{
  apr_sockaddr_t *sa_local;
  apr_socket_addr_get(&sa_local, APR_LOCAL, sock);
  return sa_local->sa.sin.sin_addr.s_addr;
}
