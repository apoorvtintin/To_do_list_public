

# to_do_list
A distributed to do list which can be used to keep a replicated to do list on a Fault tolerant distributed system. This System stores and keeps the To-Do-List on upto 3 replicated servers. This project was made for course 18-749 Building Reliable Distributed Systems in Summer 2020.

This involved the design and implementation of an industry-grade fault-tolerant distributed asynchronous system, with heartbeats, distributed consensus, total ordering, checkpointing, and logging to provide strong consistency for a distributed replicated application. This project involved supporting different replication styles (active, or hot-swap replication, as well as  passive, or primary-backup replication), along with mechanisms to ensure no downtime even as crash faults are injected.

---

**To compile:**

`make format; make clean; make;`

---

**To start the server:**

`./server {IP} {Port}`

---

**To start the client:**

`./client -C {Client ID} -I {Heartbeat interval} -H {Server IP} -P {Port}`

The interval is optional. If no value is given as input then default 10 sec value is set.

---
