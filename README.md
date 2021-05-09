## KE2 Fortuna Chess
Built from scratch, a fully featured chess program using bitboard and logic operations with the aim to go further than the previous Chess programs written by students for the La Fortuna. Highlights attack sets, move sets, checks, checkmates, en passants, etc. all via 64-bit bitboards.

Great care has been taken to account for all the various edge cases that present themselves when working with chess programming.

I have personally found the following of great help if you wish to improve on this work (a few very rare cases are yet to be accounted for):

- [Physical Location Bitboards](http://pages.cs.wisc.edu/~psilord/blog/data/chess-pages/physical.html)
- [Generating Legal Chess Moves Efficiently](https://peterellisjones.com/posts/generating-legal-chess-moves-efficiently/)

## Building
Simply invoke `make` using the included universal Makefile/

## Credits
- Steven Gunn (Creative Commons): Rotary encoder library, ILI934x driver, Font library
- Klaus-Peter Zauner (MIT), Nicholas Bishop (GNU GPL): Unified color library
- Klaus-Peter Zauner: Universal Makefile for the LaFortuna

## Images
![In Play](https://imgur.com/VdqdIKQ.jpg)
![Mate](https://imgur.com/zk3Q7so.jpg)
![Start Screen](https://imgur.com/0eyU6p8.jpg)
