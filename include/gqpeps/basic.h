// SPDX-License-Identifier: LGPL-3.0-only

/*
* Author: Hao-Xin Wang<wanghaoxin1996@gmail.com>
* Creation Date: 2023-08-03
*
* Description: GraceQ/VMC-PEPS project. Basic structures and classes.
*/

#ifndef GQPEPS_BASIC_H
#define GQPEPS_BASIC_H

#include <string>     // string
#include <vector>     // vector

namespace gqpeps {


enum BondOrientation {
  HORIZONTAL = 0,
  VERTICAL
};

BondOrientation Rotate(BondOrientation orient) {
  return BondOrientation(1 - (size_t) orient);
}

///<
/**from which direction, or the position
 *
 * UP:   MPS tensors are numbered from right to left
               2--t--0
                 |
                 1

 * DOWN: MPS tensors are numbered from left to right
                   1
                   |
                0--t--2

 * LEFT: MPS tensors are numbered from up to down;
 *
 * the order, left, down, right, up, follow the MPO/single layer tps indexes order
 */
enum BMPSPOSITION {
  LEFT = 0,
  DOWN,
  RIGHT,
  UP
};

/**
 * LEFT/RIGHT -> HORIZONTAL
 * UP/DOWN -> VERTICAL
 * @param post
 * @return
 */
BondOrientation Orientation(const BMPSPOSITION post) {
  return static_cast<enum BondOrientation>((static_cast<size_t>(post) % 2));
}

size_t MPOIndex(const BMPSPOSITION post) {
  return static_cast<size_t>(post);
}

BMPSPOSITION Opposite(const BMPSPOSITION post) {
  return static_cast<BMPSPOSITION>((static_cast<size_t>(post) + 2) % 4);
  switch (post) {
    case DOWN:
      return UP;
    case UP:
      return DOWN;
    case LEFT:
      return RIGHT;
    case RIGHT:
      return LEFT;
  }
}


struct TruncatePara {
  size_t D_min;
  size_t D_max;
  double trunc_err;

  TruncatePara(size_t d_min, size_t d_max, double trunc_error)
      : D_min(d_min), D_max(d_max), trunc_err(trunc_error) {}
};


}

#endif //GQPEPS_BASIC_H