# socket-dispatcher
Project shows base example how to use base technique for sending tcp/ip socket to another process
in Limux system. Also project shows how to make `SSL` connection over `tcp/ip`.
Disgram below shows simple scheme with clients host snd server host where server
host two type of process:
- Listener, see source `mylsnr.cpp` for details.
- Dispathcer, see source `mydisp.cpp` for details.
The remote process, `Client`, see source `myclnt.cpp` for details.

```
                              ┌─────────────────────────────────────────────────────────┐
                              │                                                         │
                              │                                                         │
                              │                                                         │
                              │                                     ┌─────────────┐     │
┌────────┐                    │                                     │             │     │
│        │                    │                                     │ Dispatcher  │     │
│        ├──────────┐         │                           ┌─────────►             │     │
│Client  │          │         │                           │         │             │     │
│        │          │         │  ┌────────────────┐       │         │             │     │
│        │          │         │  │                │       │         │             │     │
└────────┘          │         │  │                │       │         └─────────────┘     │
                    │         │  │                │       │                             │
                    │ TCP/IP  │  │                │       │                             │
                    └─────────┼──┤                ├───────┘                             │
                              │  │ Listner        │ UNIX Socket                         │
                              │  │                │                                     │
                    ┌─────────┼──┤                ├────────┐                            │
                    │         │  │                │        │                            │
                    │         │  │                │        │        ┌─────────────┐     │
┌────────┐          │         │  │                │        │        │             │     │
│        │          │         │  │                │        │        │ Dispatcher  │     │
│        │          │         │  └────────────────┘        │        │             │     │
│Client  ├──────────┘         │                            └────────►             │     │
│        │                    │                                     │             │     │
│        │                    │                                     │             │     │
└────────┘                    │                                     └─────────────┘     │
                              │                                                         │
                              │                                                         │
                              │                                                         │
                              │                                                         │
                              │                                                         │
                              │                                                         │
                              └─────────────────────────────────────────────────────────┘

```
