/*
* Author: Hao-Xin Wang<wanghaoxin1996@gmail.com>
* Creation Date: 2023-07-9
*
* Description: GraceQ/VMC-PEPS project. The generic PEPS class, implementation.
*/

#ifndef GRACEQ_VMC_PEPS_TPS_SAMPLE_H
#define GRACEQ_VMC_PEPS_TPS_SAMPLE_H


#include "gqpeps/two_dim_tn/tps/configuration.h"    //Configuration
#include "gqpeps/algorithm/vmc_update/tensor_network_2d.h"

std::default_random_engine random_engine;

namespace gqpeps {
template<typename TenElemT, typename QNT>
struct TPSSample {
 public:
  Configuration config;
  TensorNetwork2D<TenElemT, QNT> tn;
  TenElemT amplitude;

  static TruncatePara trun_para;

  TPSSample(const size_t rows, const size_t cols) : config(rows, cols), tn(rows, cols), amplitude(0) {}

  TPSSample(const SplitIndexTPS<TenElemT, QNT> &sitps, const Configuration &config)
      : config(config), tn(config.rows(), config.cols()) {
    tn = TensorNetwork2D<TenElemT, QNT>(sitps, config);
    tn.GrowBMPSForRow(0, trun_para);
    tn.GrowFullBTen(RIGHT, 0, 2, true);
    tn.InitBTen(LEFT, 0);
    amplitude = tn.Trace({0, 0}, HORIZONTAL);
  }

  /**
   * @param sitps
   * @param occupancy_num
   */
  void RandomInit(const SplitIndexTPS<TenElemT, QNT> &sitps,
                  const std::vector<size_t> &occupancy_num,
                  const size_t rand_seed) {
    config.Random(occupancy_num, rand_seed);
    tn = TensorNetwork2D<TenElemT, QNT>(sitps, config);
    tn.GrowBMPSForRow(0, trun_para);
    tn.GrowFullBTen(RIGHT, 0, 2, true);
    tn.InitBTen(LEFT, 0);
    amplitude = tn.Trace({0, 0}, HORIZONTAL);
  }


//  SplitIndexTPS<TenElemT, QNT> operator*(const SplitIndexTPS<TenElemT, QNT> &sitps) const {
//
//
//  }
//
//  operator SplitIndexTPS<TenElemT, QNT>() const {
//
//  }

  size_t MCCompressedKagomeLatticeLocalUpdateSweep(const SplitIndexTPS<TenElemT, QNT> &sitps,
                                                   std::uniform_real_distribution<double> &u_double) {
    size_t accept_num_site = 0, accept_num_bond = 0;
    tn.GenerateBMPSApproach(UP, trun_para);
    for (size_t row = 0; row < tn.rows(); row++) {
      tn.InitBTen(LEFT, row);
      tn.GrowFullBTen(RIGHT, row, 1, true);
      for (size_t col = 0; col < tn.cols(); col++) {
        accept_num_site += CompressedKagomeLatticeSingleSiteUpdate_({row, col}, sitps, u_double, HORIZONTAL);
        if (col < tn.cols() - 1) {
          accept_num_bond += CompressedKagomeLatticeExchangeUpdate_({row, col}, {row, col + 1}, HORIZONTAL, sitps,
                                                                    u_double);
          tn.BTenMoveStep(RIGHT);
        }
      }
      if (row < tn.rows() - 1) {
        tn.BMPSMoveStep(DOWN, trun_para);
      }
    }

    tn.DeleteInnerBMPS(LEFT);
    tn.DeleteInnerBMPS(RIGHT);

    tn.GenerateBMPSApproach(LEFT, trun_para);
    for (size_t col = 0; col < tn.cols(); col++) {
      tn.InitBTen(UP, col);
      tn.GrowFullBTen(DOWN, col, 1, true);
      for (size_t row = 0; row < tn.rows(); row++) {
        accept_num_site += CompressedKagomeLatticeSingleSiteUpdate_({row, col}, sitps, u_double, VERTICAL);
        if (row < tn.rows() - 1) {
          accept_num_bond += CompressedKagomeLatticeExchangeUpdate_({row, col}, {row + 1, col}, VERTICAL, sitps,
                                                                    u_double);
          tn.BTenMoveStep(DOWN);
        }
      }
      if (col < tn.cols() - 1) {
        tn.BMPSMoveStep(RIGHT, trun_para);
      }
    }

    tn.DeleteInnerBMPS(UP);
    return accept_num_bond;
//    return (accept_num_bond + accept_num_site) / 2; // a roughly estimation
  }


  size_t MCSequentiallyNNFlipSweep(const SplitIndexTPS<TenElemT, QNT> &sitps,
                                   std::uniform_real_distribution<double> &u_double) {
    size_t accept_num = 0;
    tn.GenerateBMPSApproach(UP, trun_para);
    for (size_t row = 0; row < tn.rows(); row++) {
      tn.InitBTen(LEFT, row);
      tn.GrowFullBTen(RIGHT, row, 2, true);
      for (size_t col = 0; col < tn.cols() - 1; col++) {
        accept_num += ExchangeUpdate({row, col}, {row, col + 1}, HORIZONTAL, sitps, u_double);
        if (col < tn.cols() - 2) {
          tn.BTenMoveStep(RIGHT);
        }
      }
      if (row < tn.rows() - 1) {
        tn.BMPSMoveStep(DOWN, trun_para);
      }
    }

    tn.DeleteInnerBMPS(LEFT);
    tn.DeleteInnerBMPS(RIGHT);

    tn.GenerateBMPSApproach(LEFT, trun_para);
    for (size_t col = 0; col < tn.cols(); col++) {
      tn.InitBTen(UP, col);
      tn.GrowFullBTen(DOWN, col, 2, true);
      for (size_t row = 0; row < tn.rows() - 1; row++) {
        accept_num += ExchangeUpdate({row, col}, {row + 1, col}, VERTICAL, sitps, u_double);
        if (row < tn.rows() - 2) {
          tn.BTenMoveStep(DOWN);
        }
      }
      if (col < tn.cols() - 1) {
        tn.BMPSMoveStep(RIGHT, trun_para);
      }
    }

    tn.DeleteInnerBMPS(UP);
    return accept_num;
  }

  bool ExchangeUpdate(const SiteIdx &site1, const SiteIdx &site2, BondOrientation bond_dir,
                      const SplitIndexTPS<TenElemT, QNT> &sitps,
                      std::uniform_real_distribution<double> &u_double) {
    if (config(site1) == config(site2)) {
      return true;
    }
    assert(sitps(site1)[config(site1)].GetIndexes() == sitps(site1)[config(site2)].GetIndexes());
    TenElemT psi_b = tn.ReplaceNNSiteTrace(site1, site2, bond_dir, sitps(site1)[config(site2)],
                                           sitps(site2)[config(site1)]);
    bool exchange;
    TenElemT &psi_a = amplitude;
    if (std::fabs(psi_b) >= std::fabs(psi_a)) {
      exchange = true;
    } else {
      double div = std::fabs(psi_b) / std::fabs(psi_a);
      double P = div * div;
      if (u_double(random_engine) < P) {
        exchange = true;
      } else {
        exchange = false;
        return exchange;
      }
    }

    std::swap(config(site1), config(site2));
//    const size_t config_tmp = config(site1);
//    config(site1) = config(site2);
//    config(site2) = config_tmp;
    tn.UpdateSiteConfig(site1, config(site1), sitps);
    tn.UpdateSiteConfig(site2, config(site2), sitps);
    amplitude = psi_b;
    return exchange;
  }

 private:

  bool CompressedKagomeLatticeExchangeUpdate_(const SiteIdx &site1,
                                              const SiteIdx &site2,
                                              BondOrientation bond_dir,
                                              const SplitIndexTPS<TenElemT, QNT> &sitps,
                                              std::uniform_real_distribution<double> &u_double) {
    size_t eff_config1, eff_config2;
    size_t ex_config1, ex_config2;
    size_t config1 = config(site1), config2 = config(site2);
    if (bond_dir == HORIZONTAL) {
      eff_config1 = (config1 >> 2) & 1;
      eff_config2 = config2 & 1;
      ex_config1 = config1 ^ (1 << 2);
      ex_config2 = config2 ^ 1;
    } else {
      eff_config1 = (config1 >> 1) & 1;
      eff_config2 = config2 & 1;
      ex_config1 = config1 ^ (1 << 1);
      ex_config2 = config2 ^ 1;
    }

    if (eff_config1 == eff_config2) {
      return false;
    }
    if (sitps(site1)[ex_config1].GetQNBlkNum() == 0) {
      std::cout << "warning: site (" << site1[0] << ", " << site1[1] << ") on config " << ex_config1
                << " lost tensor block." << std::endl;
      return false;
    }
    if (sitps(site2)[ex_config2].GetQNBlkNum() == 0) {
      std::cout << "warning: site (" << site1[0] << ", " << site1[1] << ") on config " << ex_config1
                << " lost tensor block." << std::endl;
      return false;
    }
    TenElemT psi_b = tn.ReplaceNNSiteTrace(site1, site2, bond_dir, sitps(site1)[ex_config1],
                                           sitps(site2)[ex_config2]);
    bool exchange;
    TenElemT &psi_a = amplitude;
    if (std::fabs(psi_b) >= std::fabs(psi_a)) {
      exchange = true;
    } else {
      double div = std::fabs(psi_b) / std::fabs(psi_a);
      double P = div * div;
      if (u_double(random_engine) < P) {
        exchange = true;
      } else {
        exchange = false;
        return exchange;
      }
    }

    config(site1) = ex_config1;
    config(site2) = ex_config2;
    tn.UpdateSiteConfig(site1, ex_config1, sitps);
    tn.UpdateSiteConfig(site2, ex_config2, sitps);
    amplitude = psi_b;
    return exchange;
  }

  bool CompressedKagomeLatticeSingleSiteUpdate_(const SiteIdx &site,
                                                const SplitIndexTPS<TenElemT, QNT> &sitps,
                                                std::uniform_real_distribution<double> &u_double,
                                                const BondOrientation mps_orient) {
    size_t config_site = config(site);
    if (config_site == 0 || config_site == 7) {
      return false;//or true
    }
    size_t rotate_config1 = config_site / 4 + 2 * (config_site % 4);
    size_t rotate_config2 = rotate_config1 / 4 + 2 * (rotate_config1 % 4);
    TenElemT psi_rotate1 = tn.ReplaceOneSiteTrace(site, sitps(site)[rotate_config1], mps_orient);
    TenElemT psi_rotate2 = tn.ReplaceOneSiteTrace(site, sitps(site)[rotate_config2], mps_orient);

    //make sure rotate1 is smaller than rotate2
    if (std::fabs(psi_rotate1) > std::fabs(psi_rotate2)) {
      std::swap(rotate_config1, rotate_config2);
      std::swap(psi_rotate1, psi_rotate2);
    }

    TenElemT psi0 = amplitude;
    double p0 = amplitude * amplitude;
    double p1 = psi_rotate1 * psi_rotate1;
    double p2 = psi_rotate2 * psi_rotate2; //p1<=p2

    if (p0 + p1 + p2 <= 2 * std::max(p0, p2)) {
      if (std::fabs(psi_rotate2) >= std::fabs(psi0)) {
        //skip to rotate2
        config(site) = rotate_config2;
        tn.UpdateSiteConfig(site, rotate_config2, sitps);
        amplitude = psi_rotate2;
        return true;
      }
      // psi0 is the largest amplitude
      double rand_num = u_double(random_engine);
      if (rand_num < p2 / p0) {
        //skip to rotate2
        config(site) = rotate_config2;
        tn.UpdateSiteConfig(site, rotate_config2, sitps);
        amplitude = psi_rotate2;
        return true;

      } else if (rand_num < (p1 + p2) / p0) {
        //skip tp rotate1
        config(site) = rotate_config1;
        tn.UpdateSiteConfig(site, rotate_config1, sitps);
        amplitude = psi_rotate1;
        return true;
      } else {
        return false;
      }
    } else { //p_middle + p_small > p_large
      double rand_num = u_double(random_engine);
      if (p0 >= p2) { //p0 = p_large, p2 = p_middle, p1 = p_small
        if (rand_num < p2 / (p2 + p1)) {
          //skip to rotate2
          config(site) = rotate_config2;
          tn.UpdateSiteConfig(site, rotate_config2, sitps);
          amplitude = psi_rotate2;
          return true;
        } else {
          //skip tp rotate1
          config(site) = rotate_config1;
          tn.UpdateSiteConfig(site, rotate_config1, sitps);
          amplitude = psi_rotate1;
          return true;
        }
      } else if (p0 <= p1) { //p0,p1,p2: small, middle large
        if (rand_num < p2 / (p0 + p1)) {
          //skip to rotate2, large
          config(site) = rotate_config2;
          tn.UpdateSiteConfig(site, rotate_config2, sitps);
          amplitude = psi_rotate2;
          return true;
        } else {
          //skip tp rotate1, middle
          config(site) = rotate_config1;
          tn.UpdateSiteConfig(site, rotate_config1, sitps);
          amplitude = psi_rotate1;
          return true;
        }
      } else { //p1 < p0 < p2
        if (rand_num <= p2 / (p0 + p1)) {
          //skip to rotate2, largest amplitude configuration
          config(site) = rotate_config2;
          tn.UpdateSiteConfig(site, rotate_config2, sitps);
          amplitude = psi_rotate2;
          return true;
        } else if (rand_num <= p2 / (p0 + p1) + p1 / p0 * (1 - p2 / (p0 + p1))) {
          //skip tp rotate1, smallest
          config(site) = rotate_config1;
          tn.UpdateSiteConfig(site, rotate_config1, sitps);
          amplitude = psi_rotate1;
          return true;
        } else {
          //no jump
          return false;
        }
      }
    } //end of the case p_middle + p_small > p_large
  } //CompressedKagomeLatticeSingleSiteUpdate_

}; //TPSSample

template<typename TenElemT, typename QNT>
TruncatePara TPSSample<TenElemT, QNT>::trun_para = TruncatePara(0, 0, 0.0);
}//gqpeps

#endif //GRACEQ_VMC_PEPS_TPS_SAMPLE_H
