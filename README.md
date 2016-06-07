Tablebases for lichess.org
==========================

[![Build Status](https://travis-ci.org/niklasf/Fathom.svg?branch=lichess)](https://travis-ci.org/niklasf/Fathom)

HTTP API for Gaviota and Syzygy tablebases. Based on
[Fathom](https://github.com/basil00/Fathom) by basil00, which is in turn based
on Ronald de Man's
[original Syzygy probing code](https://github.com/syzygy1/tb), and
[libgtb](https://github.com/michiguel/Gaviota-Tablebases)
by Miguel A. Ballicora.

Building
--------

Install libevent2:

    sudo apt-get install build-essential libevent-dev

Build and install libgtb:

    git clone https://github.com/michiguel/Gaviota-Tablebases.git
    cd Gaviota-Tablebases
    make
    sudo make install

Finally:

    make

Downloading tablebases
----------------------

Via BitTorrent: http://oics.olympuschess.com/tracker/index.php

Usage
-----

    ./fathom [--verbose] [--cors] [--port 5000]
        [--gaviota path/to/another/dir]
        [--syzygy path/to/another/dir]

HTTP API
--------

CORS enabled for all domains. Provide `callback` parameter to use JSONP.

### `GET /tablebase`

```
> curl https://expl.lichess.org/tablebase?fen=8/6B1/8/8/B7/8/K1pk4/8%20b%20-%20-%200%201
```

name | type | default | description
--- | --- | --- | ---
**fen** | string | required | FEN of the position to look up.

```javascript
{
  "checkmate": false,
  "stalemate": false,
  "insufficient_material": false,
  "dtz": -101,
  "wdl": -1,
  "real_wdl": -1,
  "moves": [
    {"uci": "c2c1n", "san": "c1=N+", "checkmate": false, "stalemate": false, "insufficient_material": false, "zeroing": true, "dtz": 109, "wdl": 1, "real_wdl": 1, "dtm": 133},
    {"uci": "c2c1r", "san": "c1=R", "checkmate": false, "stalemate": false, "insufficient_material": false, "zeroing": true, "dtz": 3, "wdl": 2, "real_wdl": 2, "dtm": 39},
    {"uci": "c2c1b", "san": "c1=B", "checkmate": false, "stalemate": false, "insufficient_material": false, "zeroing": true, "dtz": 2, "wdl": 2, "real_wdl": 2, "dtm": 39},
    {"uci": "c2c1q", "san": "c1=Q", "checkmate": false, "stalemate": false, "insufficient_material": false, "zeroing": true, "dtz": 2, "wdl": 2, "real_wdl": 2, "dtm": 39},
    {"uci": "d2d3", "san": "Kd3", "checkmate": false, "stalemate": false, "insufficient_material": false, "zeroing": false, "dtz": 3, "wdl": 2, "real_wdl": 2, "dtm": 35},
    {"uci": "d2c1", "san": "Kc1", "checkmate": false, "stalemate": false, "insufficient_material": false, "zeroing": false, "dtz": 5, "wdl": 2, "real_wdl": 2, "dtm": 31},
    {"uci": "d2d1", "san": "Kd1", "checkmate": false, "stalemate": false, "insufficient_material": false, "zeroing": false, "dtz": 3, "wdl": 2, "real_wdl": 2, "dtm": 31},
    {"uci": "d2e1", "san": "Ke1", "checkmate": false, "stalemate": false, "insufficient_material": false, "zeroing": false, "dtz": 1, "wdl": 2, "real_wdl": 2, "dtm": 31},
    {"uci": "d2e2", "san": "Ke2", "checkmate": false, "stalemate": false, "insufficient_material": false, "zeroing": false, "dtz": 1, "wdl": 2, "real_wdl": 2, "dtm": 31},
    {"uci": "d2e3", "san": "Ke3", "checkmate": false, "stalemate": false, "insufficient_material": false, "zeroing": false, "dtz": 1, "wdl": 2, "real_wdl": 2, "dtm": 31}
  ]
}
```

or `HTTP 404` if the position is not found in the tablebase, for example if a
position with castling rights or more than 6 pieces was requested. DTM values
are `null` if not a forced mate or position not found in the Gaviota
tablebases.

Moves are sorted, best move first.

License
-------

Copyright (c) 2013-2015 Ronald de Man (original code)  
Copyright (c) 2015 basil (new modifications)  
Copyright (c) 2016 Niklas Fiekas (probe server)

Ronald de Man's original code can be "redistributed and/or modified without
restrictions".

The new modifications are released under the permissive MIT License:

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
