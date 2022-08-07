# socket-dispatcher
Project shows base example how to use base technique for sending tcp/ip socket to another process
in Limux system. Also project shows how to make `SSL` connection over `tcp/ip`.
Disgram below shows simple scheme with clients hosts snd server host.

Host has two type of processes:
- _Listener_, see source `mylsnr.cpp` for details.
- _Dispathcer_, see source `mydisp.cpp` for details.

The remote process,
- _Client_, see source `myclnt.cpp` for details.

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
