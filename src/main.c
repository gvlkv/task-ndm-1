#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

static const char *bridge_type = "bridge";
static const size_t bridge_type_size = 7;

static int help(const char *exe_name) {
  printf("help: %1$s addbr BRIDGE  -- Create bridge\n"
         "      %1$s delbr BRIDGE  -- Delete BRIDGE\n"
         "      %1$s show   -- Show a list of bridges\n"
         "      %1$s addif BRIDGE IFACE\n"
         "      %1$s delif BRIDGE IFACE\n",
         exe_name);
  return 0;
}

struct ndm_hdr {
  struct nlmsghdr nl_hdr;
  struct ifinfomsg info_hdr;
};

static int bridge_add(const char *name) {
  int fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
  if (fd < 0) {
    perror("Cannot open netlink socket");
    return -1;
  }

  struct sockaddr_nl sa = {
      .nl_family = AF_NETLINK,
  };

  struct nlattr attr_ifname = {.nla_len = NLA_HDRLEN + strlen(name) + 1,
                               .nla_type = IFLA_IFNAME};
  struct nlattr attr_linkinfo = {.nla_len = NLA_HDRLEN * 2 + bridge_type_size,
                                 .nla_type = IFLA_LINKINFO};
  struct nlattr attr_infokind = {.nla_len = NLA_HDRLEN + bridge_type_size,
                                 .nla_type = IFLA_INFO_KIND};
  uint8_t buff_nla[512] = {};
  memcpy(buff_nla, &attr_ifname, sizeof(attr_ifname));
  memcpy(buff_nla + NLA_HDRLEN, name, strlen(name) + 1);
  memcpy(buff_nla + NLA_ALIGN(attr_ifname.nla_len), &attr_linkinfo,
         sizeof(attr_linkinfo));
  memcpy(buff_nla + NLA_ALIGN(attr_ifname.nla_len) + NLA_HDRLEN, &attr_infokind,
         sizeof(attr_infokind));
  memcpy(buff_nla + NLA_ALIGN(attr_ifname.nla_len) + NLA_HDRLEN * 2,
         bridge_type, bridge_type_size);
  uint32_t nla_len = NLA_ALIGN(attr_ifname.nla_len) + NLA_HDRLEN * 2 +
                     NLA_ALIGN(bridge_type_size);

  uint32_t message_len =
      nla_len + sizeof(struct nlmsghdr) + sizeof(struct ifinfomsg);

  uint8_t buff_iov[512] = {};
  memcpy(buff_iov,
         &(struct ndm_hdr){.nl_hdr.nlmsg_len = message_len,
                           .nl_hdr.nlmsg_type = RTM_NEWLINK,
                           .nl_hdr.nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE,
                           .nl_hdr.nlmsg_seq = (uint32_t)time(NULL),
                           .info_hdr.ifi_family = AF_UNSPEC},
         sizeof(struct ndm_hdr));
  memcpy(buff_iov + sizeof(struct ndm_hdr), buff_nla, nla_len); // TODO size
  struct iovec iov = {.iov_base = buff_iov, .iov_len = message_len};
  struct msghdr mh = {
      .msg_name = &sa,
      .msg_namelen = sizeof(sa),
      .msg_iov = &iov,
      .msg_iovlen = 1,
  };
  sendmsg(fd, &mh, 0);

  close(fd);

  return 0;
}

static int bridge_add_iface(const char *br_name, const char *if_name) {
  int fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
  if (fd < 0) {
    perror("Cannot open netlink socket");
    return -1;
  }

  struct sockaddr_nl sa = {
      .nl_family = AF_NETLINK,
  };

  struct nlattr attr_master = {.nla_len = 8, .nla_type = IFLA_MASTER};
  uint8_t buff_nla[512] = {};
  memcpy(buff_nla, &attr_master, sizeof(attr_master));
  uint32_t br_index = if_nametoindex(br_name);
  memcpy(buff_nla + NLA_HDRLEN, &br_index, 4);
  uint32_t nla_len = NLA_HDRLEN + 4;

  uint32_t message_len =
      nla_len + sizeof(struct nlmsghdr) + sizeof(struct ifinfomsg);

  uint8_t buff_iov[512] = {};
  memcpy(buff_iov,
         &(struct ndm_hdr){.nl_hdr.nlmsg_len = message_len,
                           .nl_hdr.nlmsg_type = RTM_NEWLINK,
                           .nl_hdr.nlmsg_flags = NLM_F_REQUEST,
                           .nl_hdr.nlmsg_seq = (uint32_t)time(NULL),
                           .info_hdr.ifi_family = AF_UNSPEC,
                           .info_hdr.ifi_index = if_nametoindex(if_name)},
         sizeof(struct ndm_hdr));
  memcpy(buff_iov + sizeof(struct ndm_hdr), buff_nla, nla_len); // TODO size
  struct iovec iov = {.iov_base = buff_iov, .iov_len = message_len};
  struct msghdr mh = {
      .msg_name = &sa,
      .msg_namelen = sizeof(sa),
      .msg_iov = &iov,
      .msg_iovlen = 1,
  };
  sendmsg(fd, &mh, 0);

  close(fd);

  return 0;
}

static int bridge_del(const char *name) {
  int fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
  if (fd < 0) {
    perror("Cannot open netlink socket");
    return -1;
  }

  struct sockaddr_nl sa = {
      .nl_family = AF_NETLINK,
  };

  struct nlattr attr_linkinfo = {.nla_len = NLA_HDRLEN * 2 + bridge_type_size,
                                 .nla_type = IFLA_LINKINFO};
  struct nlattr attr_infokind = {.nla_len = NLA_HDRLEN + bridge_type_size,
                                 .nla_type = IFLA_INFO_KIND};
  uint8_t buff_nla[512] = {};
  memcpy(buff_nla, &attr_linkinfo, sizeof(attr_linkinfo));
  memcpy(buff_nla + NLA_HDRLEN, &attr_infokind, sizeof(attr_infokind));
  memcpy(buff_nla + NLA_HDRLEN * 2, bridge_type, bridge_type_size);
  uint32_t nla_len = NLA_HDRLEN * 2 + NLA_ALIGN(bridge_type_size);

  uint32_t message_len =
      nla_len + sizeof(struct nlmsghdr) + sizeof(struct ifinfomsg);

  uint8_t buff_iov[512] = {};
  memcpy(buff_iov,
         &(struct ndm_hdr){.nl_hdr.nlmsg_len = message_len,
                           .nl_hdr.nlmsg_type = RTM_DELLINK,
                           .nl_hdr.nlmsg_flags = NLM_F_REQUEST,
                           .nl_hdr.nlmsg_seq = (uint32_t)time(NULL),
                           .info_hdr.ifi_family = AF_UNSPEC,
                           .info_hdr.ifi_index = if_nametoindex(name)},
         sizeof(struct ndm_hdr));
  memcpy(buff_iov + sizeof(struct ndm_hdr), buff_nla, nla_len); // TODO size
  struct iovec iov = {.iov_base = buff_iov, .iov_len = message_len};
  struct msghdr mh = {
      .msg_name = &sa,
      .msg_namelen = sizeof(sa),
      .msg_iov = &iov,
      .msg_iovlen = 1,
  };
  sendmsg(fd, &mh, 0);

  close(fd);

  return 0;
}

static int bridge_del_iface(const char *br_name, const char *if_name) {
  return 0;
}

int main(int argc, char **argv) {
  if (argc == 3) {
    if (!strcmp(argv[1], "addbr"))
      return bridge_add(argv[2]);
    if (!strcmp(argv[1], "delbr"))
      return bridge_del(argv[2]);
  }
  if (argc == 4) {
    if (!strcmp(argv[1], "addif"))
      return bridge_add_iface(argv[2], argv[3]);
    if (!strcmp(argv[1], "delif"))
      return bridge_del_iface(argv[2], argv[3]);
  }
  return help(argv[0]);
}
