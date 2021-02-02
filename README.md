# peta: a radio system

## What?
This radio system consists of three parts.

[Speaker] <-> [Server] <-> [Listener]

## How?

### Server:
```
% ./station 0.0.0.0 2000
```
* 0.0.0.0: IP to serve.
* 2000: Port to serve.

### Speaker:
```
% ./cast 127.0.0.1 2000 CH_NAME
```
* 127.0.0.1: Server IP.
* 2000: Server port.
* CH_NAME: Your channel name.(ex: news-today)

### Listener:
```
% ./receive 127.0.0.1 2000 CH_NAME
```
* 127.0.0.1: Server IP.
* 2000: Server port.
* CH_NAME: A channel name you listen.

[or]

Access to the URL in your browser.
```
http://127.0.0.1:2000/CH_NAME
```
* /CH_NAME: A channel name.(Must start with '/')

## Lisence
BSD

## Author
[NaturalStyle PREMIUM](https://p.na-s.jp/)
