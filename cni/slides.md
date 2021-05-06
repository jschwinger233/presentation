
# CNI
## Container Network Interface


By @zc

---

# What is CNI?

---

## A Grave Issue Within Container Cluster

![](https://raw.githubusercontent.com/jschwinger23/presentation/master/cni/Docker-Bridge-Networking.png)

---

## Network Solutions

- Flannel (2014-08-13)
- Weave (2014-08-29)
- ...
- Calico (????-??-??)

---
## Goals of CNI/CNM

To standardize an interface/procotol for external network provider to make integration into
- rkt (CoreOS)
- Docker (Docker Inc.)

---

## Timeline

- 2015-04-11: Docker CNM RoadMap
- 2015-05-06: rkt CNI Network Proposal
- 2016-01-14: Kubernetes posted blog "Why Kubernetes doesn’t use libnetwork"
- ...
- 2021-05-07: CNI boasts 27 3rd party plugins, while Docker lists 3 network plugin in doc

---

# Demystify CNI

---

- 4 environ variables
- 1 configure file
- 1 binary

```shell
export CNI_COMMAND=[action]
export CNI_CONTAINERID=[container_id]
export CNI_NETNS=[path_to_netns]
export CNI_IFNAME=[veth_name_inside_container]
[cni_bin] < [cni_config]
```

---

## How K8s Uses CNI (1)

```shell
$ docker run -td --name pause --net none bash bash
cd291a9e39fbbf683fcfd1410a7e7233472e0832dd512db99886d5dec2ece186

$ docker inspect pause | grep -i pid
            "Pid": 924986,
            "PidMode": "",
            "PidsLimit": null,

$ docker exec pause ip a
1: lo: <LOOPBACK,UP,LOWER_UP> mtu 65536 qdisc noqueue state UNKNOWN group default qlen 1000
    link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00
    inet 127.0.0.1/8 scope host lo
       valid_lft forever preferred_lft forever
```

---

## How K8s Uses CNI (2)

```shell
$ CNI_COMMAND=ADD \
CNI_CONTAINERID=cd291a9e39fbbf683fcfd1410a7e7233472e0832dd512db99886d5dec2ece186 \
CNI_NETNS=/proc/924986/ns/net \
CNI_IFNAME=eth0 \
CNI_PATH=/opt/cni/bin/ \
/opt/cni/bin/calico < /etc/cni/net.d/10-caliico.conf 2> /dev/null
{
    "cniVersion": "0.3.0",
    "interfaces": [
        {
            "name": "calicd291a9e39f"
        }
    ],
    "ips": [
        {
            "version": "4",
            "address": "192.168.118.128/32"
        }
    ],
    "dns": {}
}

$ docker exec pause ip a
1: lo: <LOOPBACK,UP,LOWER_UP> mtu 65536 qdisc noqueue state UNKNOWN group default qlen 1000
    link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00
    inet 127.0.0.1/8 scope host lo
       valid_lft forever preferred_lft forever
3: eth0@if7: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc noqueue state UP group default
    link/ether 9e:2f:c5:dc:ad:ae brd ff:ff:ff:ff:ff:ff link-netnsid 0
    inet 192.168.118.128/32 scope global eth0
       valid_lft forever preferred_lft forever
```

---

## How K8s Uses CNI (3)

```shell
$ docker run -d --name my-redis --net container:pause redis

$ docker exec my-redis ip a
1: lo: <LOOPBACK,UP,LOWER_UP> mtu 65536 qdisc noqueue state UNKNOWN group default qlen 1000
    link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00
    inet 127.0.0.1/8 scope host lo
       valid_lft forever preferred_lft forever
3: eth0@if7: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc noqueue state UP group default
    link/ether 9e:2f:c5:dc:ad:ae brd ff:ff:ff:ff:ff:ff link-netnsid 0
    inet 192.168.118.128/32 scope global eth0
       valid_lft forever preferred_lft forever
```

---

## More details (that you needn't know)

- 2 optional env
- 4 actions
- configure spec
- stdout / stderr

---

# Integration Into Docker

---

- Why?

    Because CNM is fading away, yet Eru is still counting on Docker to create container.

- Why so hard? Why K8s made it easily?

    ~~Because this piece of **** abandoned Docker in favor of Containerd.~~

    ~~Because Kubernetes-kubelet calls CNI on its own, independent to the actual runtimes.~~

---

## The Original Sin of CNI

---

CNI is claimed to be runtime-neutral and orchestrator-neutral, but initially it's design merely for rkt, whose sales point is **pod-native**, which fits in with CNI in nature.

Let me get this clear: CNI IS DESIGNED FOR **POD** MODEL! IT'S NOT RUNTIME-NEUTRAL AT ALL!

That's the root cause of why K8s could integrate with CNI smoothly but Docker suffers.

Hint: `CNI_NETNS` and `CNI_CONTAINERID`

---

## Key Question to Implementation: When to call CNI

---

![](https://raw.githubusercontent.com/jschwinger23/presentation/master/cni/docker-containerd-shim.png)

1. Before Dockerd: Barrel
2. Inside Dockerd: another CNM plugin

---

## Surprising Answer: Between shim and runc

---

![](https://raw.githubusercontent.com/jschwinger23/presentation/master/cni/docker-containerd-shim.png)

- originally: shim -> runc
- my proposal: shim -> **our oci wrapper** -> runc
---

## How?

```
service Containers {
	rpc Create(CreateContainerRequest) returns (CreateContainerResponse);
}

message Container {
	string id = 1;
	map<string, string> labels  = 2;
	string image = 3;
	message Runtime {
		string name = 1;
		google.protobuf.Any options = 2;
	}
	Runtime runtime = 4;
	google.protobuf.Any spec = 5;
	string snapshotter = 6;
	string snapshot_key = 7;
	google.protobuf.Timestamp created_at = 8 [(gogoproto.stdtime) = true, (gogoproto.nullable) = false];
	google.protobuf.Timestamp updated_at = 9 [(gogoproto.stdtime) = true, (gogoproto.nullable) = false];
	map<string, google.protobuf.Any> extensions = 10 [(gogoproto.nullable) = false];
}
```

---

## Let me demonstrate it proudly (1)

```json
# /etc/docker/daemon.json
{
    "hosts": ["unix:///var/run/docker.sock", "tcp://0.0.0.0:2376"],
    "cluster-store": "etcd://localhost:2379",
    "runtimes": {
        "cni": {
            "path": "/usr/local/bin/docker-cni",
            "runtimeArgs": [
                "--config",
                "/etc/docker/cni.yaml",
                "--runtime-path",
                "/usr/bin/runc",
                "--"
            ]
        }
    }
}
```

---

## Let me demonstrate it proudly (2)

```shell
$ docker run -td --name zc --net none --runtime cni bash bash
0bc7363cd04749ea9544cd8ac7d23949a71b6205d7376619fb3e95bd80dd8df1

$ docker exec zc ip a
1: lo: <LOOPBACK,UP,LOWER_UP> mtu 65536 qdisc noqueue state UNKNOWN qlen 1000
    link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00
    inet 127.0.0.1/8 scope host lo
       valid_lft forever preferred_lft forever
3: eth0@if6: <BROADCAST,MULTICAST,UP,LOWER_UP,M-DOWN> mtu 1500 qdisc noqueue state UP
    link/ether ae:9f:31:53:8a:40 brd ff:ff:ff:ff:ff:ff
    inet 192.168.118.129/32 scope global eth0
       valid_lft forever preferred_lft forever
```

---

## More details (that you needn't know neither)

```json
# oci spec
# /run/containerd/io.containerd.runtime.v2.task/moby/0bc7363cd04749ea9544cd8ac7d23949a71b6205d7376619fb3e95bd80dd8df1/config.json
{
    "namespaces": [
      {
        "type": "network",
        "path": "/var/run/netns/0bc7363cd047"
      }
    ],

  "hooks": {
    "poststop": [
      {
        "path": "/bin/bash",
        "args": [
          "bash",
          "-c",
          "/opt/cni/bin/calico  <<<'{\"name\":\"calico\",\"cniVersion\":\"0.3.0\",\"type\":\"calico\",\"log_level\":\"INFO\",\"etcd_endpoints\":\"http://127.0.0.1:2379\",\"log_file_path\":\"/var/log/calico/cni/cni.log\",\"ipam\":{\"type\":\"calico-ipam\"}}'"
        ],
        "env": [
          "CNI_COMMAND=DEL",
          "CNI_CONTAINERID=0bc7363cd04749ea9544cd8ac7d23949a71b6205d7376619fb3e95bd80dd8df1",
          "CNI_NETNS=/var/run/netns/0bc7363cd047",
          "CNI_IFNAME=eth0",
          "CNI_PATH=/opt/cni/bin/",
          "CNI_ARGS=PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin;HOSTNAME=0bc7363cd047;TERM=xterm;_BASH_VERSION=5.1.4;_BASH_BASELINE=5.1;_BASH_LATEST_PATCH=4"
        ]
      },
      {
        "path": "/bin/bash",
        "args": [
          "bash",
          "-c",
          "ip net d 0bc7363cd047"
        ]
      }
    ]
  },
}
```

---

# What's more

- Fixed-IP: `docker restart` will guarantee the same IP addresss

---

# Thanks
