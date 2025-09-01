# Not your GIMP

University project, simple man-in-the-middle proxy with basic features such as editing the request or the response.

Tested to have browser compatibility.

 ## Proxy Flow (Sequence Diagram)

```mermaid
sequenceDiagram
    participant C as Client
    participant P as ProxyServer
    participant S as Target Server

    C->>P: TCP connect()
    P->>P: accept() new socket
    Note right of P: Spawn handler thread

    C->>P: HTTP Request
    P->>P: handleClient()<br/>Parse Host header
    P->>S: TCP connect()
    S-->>P: Connection established

    Note right of P: Spawn forwardData() threads

    P->>S: Forward Request
    S-->>P: HTTP Response
    P->>P: Inject HTML (optional)
    P->>C: Forward Response

    P->>P: Log [request] and [response]
    C->>P: close()
    P->>P: close sockets
