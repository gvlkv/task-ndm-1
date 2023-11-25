#include <dirent.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define PR_ERROR(...)                                                          \
  do {                                                                         \
    fprintf(stderr, __VA_ARGS__);                                              \
    exit(-1);                                                                  \
  } while (0)

enum { buf_size = 512 };

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

static void check_iface_name(const char *name, const char *type) {
  if (strlen(name) >= IF_NAMESIZE) {
    PR_ERROR("%s name too long (>=%d).\n", type, IF_NAMESIZE);
  }
}

static int cmd_bridge_add(const char *name) {
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
  uint8_t buff_nla[buf_size] = {};
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

  uint8_t buff_iov[buf_size] = {};
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

static int cmd_bridge_del(const char *name) {
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
  uint8_t buff_nla[buf_size] = {};
  memcpy(buff_nla, &attr_linkinfo, sizeof(attr_linkinfo));
  memcpy(buff_nla + NLA_HDRLEN, &attr_infokind, sizeof(attr_infokind));
  memcpy(buff_nla + NLA_HDRLEN * 2, bridge_type, bridge_type_size);
  uint32_t nla_len = NLA_HDRLEN * 2 + NLA_ALIGN(bridge_type_size);

  uint32_t message_len =
      nla_len + sizeof(struct nlmsghdr) + sizeof(struct ifinfomsg);

  uint8_t buff_iov[buf_size] = {};
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

static int bridge_mod_iface(unsigned br_index, unsigned if_index);

static int cmdbridge_add_iface(const char *br_name, const char *if_name) {
  return bridge_mod_iface(if_nametoindex(br_name), if_nametoindex(if_name));
}

static int cmd_bridge_del_iface(const char *if_name) {
  return bridge_mod_iface(0, if_nametoindex(if_name));
};

static int bridge_mod_iface(unsigned br_index, unsigned if_index) {
  int fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
  if (fd < 0) {
    perror("Cannot open netlink socket");
    return -1;
  }

  struct sockaddr_nl sa = {
      .nl_family = AF_NETLINK,
  };

  struct nlattr attr_master = {.nla_len = 8, .nla_type = IFLA_MASTER};
  uint8_t buff_nla[buf_size] = {};
  memcpy(buff_nla, &attr_master, sizeof(attr_master));
  memcpy(buff_nla + NLA_HDRLEN, &br_index, 4);
  uint32_t nla_len = NLA_HDRLEN + 4;

  uint32_t message_len =
      nla_len + sizeof(struct nlmsghdr) + sizeof(struct ifinfomsg);

  uint8_t buff_iov[buf_size] = {};
  memcpy(buff_iov,
         &(struct ndm_hdr){.nl_hdr.nlmsg_len = message_len,
                           .nl_hdr.nlmsg_type = RTM_NEWLINK,
                           .nl_hdr.nlmsg_flags = NLM_F_REQUEST,
                           .nl_hdr.nlmsg_seq = (uint32_t)time(NULL),
                           .info_hdr.ifi_family = AF_UNSPEC,
                           .info_hdr.ifi_index = if_index},
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

static bool file_exists(const char *path) {
  struct stat stat_buf;
  return (stat(path, &stat_buf) == 0);
}

static bool iface_exists(const char *name) {
  char buf[buf_size];
  sprintf(buf, "/sys/class/net/%s", name);
  return file_exists(buf);
}

static bool iface_is_bridge(const char *iface_name) {
  if (!iface_exists(iface_name))
    return false;
  char buf[buf_size];
  sprintf(buf, "/sys/class/net/%s/bridge", iface_name);
  return file_exists(buf);
}

static bool iface_is_up(const char *name) {
  if (!iface_exists(name))
    return false;
  char buf[buf_size];
  sprintf(buf, "/sys/class/net/%s/flags", name);
  FILE *flags_file;
  flags_file = fopen(buf, "r");
  if (!flags_file)
    return false;
  int flags = 0;
  fscanf(flags_file, "%x", &flags);
  fclose(flags_file);
  return (flags & 1); // up/down flag
}

static bool iface_is_some_bridge_member(const char *name) {
  if (!iface_exists(name))
    return false;
  char buf[buf_size];
  sprintf(buf, "/sys/class/net/%s/master/bridge/bridge_id", name);
  return file_exists(buf);
}

static bool iface_is_bridge_member(const char *ifname, const char *brname) {
  if (!iface_exists(ifname))
    return false;
  if (!iface_is_bridge(brname))
    return false;
  char buf[buf_size];
  sprintf(buf, "/sys/class/net/%s/lower_%s", brname, ifname);
  return file_exists(buf);
}

static bool bridge_is_stp_enabled(const char *name) {
  if (!iface_is_bridge(name))
    return false;
  char buf[buf_size];
  sprintf(buf, "/sys/class/net/%s/bridge/stp_state", name);
  FILE *flags_file;
  flags_file = fopen(buf, "r");
  if (!flags_file)
    return false;
  int flags = 0;
  fscanf(flags_file, "%d", &flags);
  fclose(flags_file);
  return (flags == 1);
}

static void bridge_get_id(const char *name, char *out) {
  if (!iface_is_bridge(name))
    return;
  char buf[buf_size];
  sprintf(buf, "/sys/class/net/%s/bridge/bridge_id", name);
  FILE *flags_file;
  flags_file = fopen(buf, "r");
  if (!flags_file)
    return;
  fscanf(flags_file, "%s", out);
  fclose(flags_file);
}

typedef void (*dir_callback_t)(const char *fname);
static void dir_for_each_entry(const char *path, dir_callback_t callback) {
  DIR *dir;
  struct dirent *ep;
  dir = opendir(path);
  if (dir != NULL) {
    while ((ep = readdir(dir)) != NULL) {
      const char *fname = ep->d_name;
      callback(fname);
    }
    closedir(dir);
  } else {
    PR_ERROR("Couldn't open sysfs directory.\n");
  }
}

static bool print_ifaces_cb_first_line;
static void print_ifaces_cb(const char *fname) {
  if (strstr(fname, "lower_") == fname) {
    if (!print_ifaces_cb_first_line)
      printf("\n\t\t\t\t\t\t\t");
    printf("\t%s", fname + 6);
    print_ifaces_cb_first_line = false;
  }
}

static void cmd_show_callback(const char *ifname) {
  if (iface_is_bridge(ifname)) {
    char bridge_id[buf_size] = {};
    bridge_get_id(ifname, bridge_id);
    bool stp = bridge_is_stp_enabled(ifname);
    bool state = iface_is_up(ifname);
    printf("%s\t\t%s\t%s\t\t%s", ifname, bridge_id, stp ? "yes" : "no",
           state ? "up" : "down");
    char buf[buf_size];
    sprintf(buf, "/sys/class/net/%s", ifname);
    print_ifaces_cb_first_line = true;
    dir_for_each_entry(buf, print_ifaces_cb);
    printf("\n");
  }
}

static int cmd_show(void) {
  printf("bridge name\tbridge id\t\tSTP enabled\tstate\tinterfaces\n");
  dir_for_each_entry("/sys/class/net", cmd_show_callback);
  return 0;
}

int main(int argc, char **argv) {
  const char *prog = argv[0];

  if (argc == 2 && !strcmp(argv[1], "show")) {
    return cmd_show();
  }
  if (argc == 3) {
    const char *command = argv[1];
    const char *brname = argv[2];
    check_iface_name(brname, "Bridge");

    if (!strcmp(command, "addbr")) {
      if (iface_exists(brname))
        PR_ERROR("Interface exists.\n");
      if (cmd_bridge_add(brname) || !iface_is_bridge(brname))
        PR_ERROR("Error adding bridge.\n");
      else
        return 0;
    }
    if (!strcmp(command, "delbr")) {
      if (!iface_exists(brname))
        PR_ERROR("Interface does not exist.\n");
      if (!iface_is_bridge(brname))
        PR_ERROR("Interface is not a bridge.\n");
      if (iface_is_up(brname))
        PR_ERROR("Interface is up.\n");
      if (cmd_bridge_del(brname) || iface_exists(brname))
        PR_ERROR("Error deleting bridge.\n");
      else
        return 0;
    }
  }
  if (argc == 4) {
    const char *command = argv[1];
    const char *brname = argv[2];
    const char *ifname = argv[3];
    check_iface_name(brname, "Bridge");
    check_iface_name(ifname, "Interface");

    if (!iface_exists(ifname))
      PR_ERROR("Interface does not exist.\n");
    if (!iface_is_bridge(brname))
      PR_ERROR("Bridge does not exist.\n");
    if (iface_is_bridge(ifname))
      PR_ERROR("%s is a bridge.\n", ifname);

    if (!strcmp(command, "addif")) {
      if (iface_is_bridge_member(ifname, brname))
        PR_ERROR("Interface %s already connected to bridge %s.\n", ifname,
                 brname);
      if (iface_is_some_bridge_member(ifname))
        PR_ERROR("Interface connected to another bridge.\n");
      if (cmdbridge_add_iface(brname, ifname) ||
          !iface_is_bridge_member(ifname, brname))
        PR_ERROR("Error adding interface to bridge.\n");
      return 0;
    }
    if (!strcmp(command, "delif")) {
      if (!iface_is_bridge_member(ifname, brname))
        PR_ERROR("Interface %s not connected to bridge %s.\n", ifname, brname);
      if (cmd_bridge_del_iface(ifname) ||
          iface_is_bridge_member(ifname, brname))
        PR_ERROR("Error deleting interface from bridge.\n");
      return 0;
    }
  }

  return help(prog);
}
