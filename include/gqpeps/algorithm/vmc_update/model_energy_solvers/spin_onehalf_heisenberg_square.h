/*
* Author: Hao-Xin Wang<wanghaoxin1996@gmail.com>
* Creation Date: 2023-09-12
*
* Description: GraceQ/VMC-PEPS project. Model Energy Solver for spin-1/2 AFM Heisenberg model in square lattice
*/

#ifndef GRACEQ_VMC_PEPS_SPIN_ONEHALF_HEISENBERG_SQUARE_H
#define GRACEQ_VMC_PEPS_SPIN_ONEHALF_HEISENBERG_SQUARE_H

#include "gqpeps/algorithm/vmc_update/model_energy_solver.h"      //ModelEnergySolver
#include "gqpeps/algorithm/vmc_update/model_measurement_solver.h" // ModelMeasurementSolver

namespace gqpeps {
using namespace gqten;

template<typename TenElemT, typename QNT>
class SpinOneHalfHeisenbergSquare : public ModelEnergySolver<TenElemT, QNT>, ModelMeasurementSolver<TenElemT, QNT> {
  using SITPS = SplitIndexTPS<TenElemT, QNT>;
 public:
  using ModelEnergySolver<TenElemT, QNT>::ModelEnergySolver;

  template<typename WaveFunctionComponentType, bool calchols = true>
  TenElemT CalEnergyAndHoles(
      const SITPS *sitps,
      WaveFunctionComponentType *tps_sample,
      TensorNetwork2D<TenElemT, QNT> &hole_res
  );

  template<typename WaveFunctionComponentType>
  ObservablesLocal<TenElemT> SampleMeasure(
      const SITPS *sitps,
      WaveFunctionComponentType *tps_sample
  );
};

template<typename TenElemT, typename QNT>
template<typename WaveFunctionComponentType, bool calchols>
TenElemT SpinOneHalfHeisenbergSquare<TenElemT, QNT>::CalEnergyAndHoles(const SITPS *split_index_tps,
                                                                       WaveFunctionComponentType *tps_sample,
                                                                       TensorNetwork2D<TenElemT, QNT> &hole_res) {
  TenElemT energy(0);
  TensorNetwork2D<TenElemT, QNT> &tn = tps_sample->tn;
  const Configuration &config = tps_sample->config;
  const BMPSTruncatePara &trunc_para = SquareTPSSampleNNFlip<TenElemT, QNT>::trun_para;
  TenElemT inv_psi = 1.0 / (tps_sample->amplitude);
  tn.GenerateBMPSApproach(UP, trunc_para);
  for (size_t row = 0; row < tn.rows(); row++) {
    tn.InitBTen(LEFT, row);
    tn.GrowFullBTen(RIGHT, row, 1, true);
    // update the amplitude so that the error of ratio of amplitude can reduce by cancellation.
    tps_sample->amplitude = tn.Trace({row, 0}, HORIZONTAL);
    inv_psi = 1.0 / tps_sample->amplitude;
    for (size_t col = 0; col < tn.cols(); col++) {
      const SiteIdx site1 = {row, col};
      //Calculate the holes
      if constexpr (calchols) {
        hole_res(site1) = Dag(tn.PunchHole(site1, HORIZONTAL));
      }
      if (col < tn.cols() - 1) {
        //Calculate horizontal bond energy contribution
        const SiteIdx site2 = {row, col + 1};
        if (config(site1) == config(site2)) {
          energy += 0.25;
        } else {
          TenElemT psi_ex = tn.ReplaceNNSiteTrace(site1, site2, HORIZONTAL,
                                                  (*split_index_tps)(site1)[config(site2)],
                                                  (*split_index_tps)(site2)[config(site1)]);
          if (std::abs(psi_ex * inv_psi) > 1.0e8) {
            std::cerr << "psi_exchange : " << std::scientific << psi_ex
                      << ", psi_0 : " << std::scientific << tps_sample->amplitude
                      << std::endl;
          }
          energy += (-0.25 + psi_ex * inv_psi * 0.5);
        }
        tn.BTenMoveStep(RIGHT);
      }
    }
    if (row < tn.rows() - 1) {
      tn.BMPSMoveStep(DOWN, trunc_para);
    }
  }

  //Calculate vertical bond energy contribution
  tn.GenerateBMPSApproach(LEFT, trunc_para);
  for (size_t col = 0; col < tn.cols(); col++) {
    tn.InitBTen(UP, col);
    tn.GrowFullBTen(DOWN, col, 2, true);
    tps_sample->amplitude = tn.Trace({0, col}, VERTICAL);
    inv_psi = 1.0 / tps_sample->amplitude;
    for (size_t row = 0; row < tn.rows() - 1; row++) {
      const SiteIdx site1 = {row, col};
      const SiteIdx site2 = {row + 1, col};
      if (config(site1) == config(site2)) {
        energy += 0.25;
      } else {
        TenElemT psi_ex = tn.ReplaceNNSiteTrace(site1, site2, VERTICAL,
                                                (*split_index_tps)(site1)[config(site2)],
                                                (*split_index_tps)(site2)[config(site1)]);
        if (std::abs(psi_ex * inv_psi) > 1.0e8) {
          std::cerr << "psi_exchange : " << std::scientific << psi_ex
                    << ", psi_0 : " << std::scientific << tps_sample->amplitude
                    << std::endl;
        }
        energy += (-0.25 + psi_ex * inv_psi * 0.5);
      }
      if (row < tn.rows() - 2) {
        tn.BTenMoveStep(DOWN);
      }
    }
    if (col < tn.cols() - 1) {
      tn.BMPSMoveStep(RIGHT, trunc_para);
    }
  }
  return energy;
}

template<typename TenElemT, typename QNT>
template<typename WaveFunctionComponentType>
ObservablesLocal<TenElemT> SpinOneHalfHeisenbergSquare<TenElemT, QNT>::SampleMeasure(
    const SITPS *split_index_tps,
    WaveFunctionComponentType *tps_sample
) {
  ObservablesLocal<TenElemT> res;
  TenElemT energy(0);
  const double bond_energy_extremly_large = 1.0e5;
  TensorNetwork2D<TenElemT, QNT> &tn = tps_sample->tn;
  const size_t lx = tn.cols(), ly = tn.rows();
  res.bond_energys_loc.reserve(lx * ly * 2);
  res.two_point_functions_loc.reserve(lx / 2 * 3);
  const Configuration &config = tps_sample->config;
  const BMPSTruncatePara &trunc_para = SquareTPSSampleNNFlip<TenElemT, QNT>::trun_para;
  TenElemT inv_psi = 1.0 / (tps_sample->amplitude);
  tn.GenerateBMPSApproach(UP, trunc_para);
  for (size_t row = 0; row < ly; row++) {
    tn.InitBTen(LEFT, row);
    tn.GrowFullBTen(RIGHT, row, 1, true);
    // update the amplitude so that the error of ratio of amplitude can reduce by cancellation.
    tps_sample->amplitude = tn.Trace({row, 0}, HORIZONTAL);
    inv_psi = 1.0 / tps_sample->amplitude;
    for (size_t col = 0; col < lx; col++) {
      const SiteIdx site1 = {row, col};
      if (col < tn.cols() - 1) {
        //Calculate horizontal bond energy contribution
        const SiteIdx site2 = {row, col + 1};
        double horizontal_bond_energy;
        if (config(site1) == config(site2)) {
          horizontal_bond_energy = 0.25;
        } else {
          TenElemT psi_ex = tn.ReplaceNNSiteTrace(site1, site2, HORIZONTAL,
                                                  (*split_index_tps)(site1)[config(site2)],
                                                  (*split_index_tps)(site2)[config(site1)]);
          TenElemT ratio = psi_ex * inv_psi;
          if (std::abs(ratio) > bond_energy_extremly_large) {
            std::cout << "Warning for possible numeric floating point err: \n"
                      << "Site : (" << row << ", " << col << ") "
                      << "Bond Orientation :" << "Horizontal"
                      << "psi_exchange : " << std::scientific << psi_ex
                      << ", psi_0 : " << std::scientific << tps_sample->amplitude
                      << ", ratio : " << std::scientific << ratio
                      << "original tensor norms : (" << tn(site1).Norm2() << ", " << tn(site2).Norm2() << ") "
                      << "exchange tensor norms : (" << (*split_index_tps)(site1)[config(site2)].Norm2()
                      << ", " << (*split_index_tps)(site2)[config(site1)].Norm2() << ") "
                      << std::endl;
            // set the bond energy = 0.0
          } else {
            horizontal_bond_energy = (-0.25 + ratio * 0.5);
          }
        }
        energy += horizontal_bond_energy;
        res.bond_energys_loc.push_back(horizontal_bond_energy);
        tn.BTenMoveStep(RIGHT);
      }
    }
    if (row == tn.rows() / 2) { //measure correlation in the middle bonds
      SiteIdx site1 = {row, lx / 4};

      // sz(i) * sz(j)
      double sz1 = config(site1) - 0.5;
      for (size_t i = 1; i <= lx / 2; i++) {
        SiteIdx site2 = {row, lx / 4 + i};
        double sz2 = config(site2) - 0.5;
        res.two_point_functions_loc.push_back(sz1 * sz2);
      }

      std::vector<TenElemT> diag_corr(lx / 4);// sp(i) * sm(j) or sm(i) * sp(j), the valid channel
      tn(site1) = (*split_index_tps)(site1)[1 - config(site1)]; //temporally change
      tn.TruncateBTen(LEFT, lx / 4); // may be above two lines should be summarized as an API
      tn.GrowBTenStep(LEFT);
      tn.GrowFullBTen(RIGHT, row, lx / 4 + 1, false);
      tn.GrowFullBTen(LEFT, row, lx / 4 + 1, false);
      for (size_t i = 1; i <= lx / 2; i++) {
        SiteIdx site2 = {row, lx / 4 + i};
        //sm(i) * sp(j)
        if (config(site2) == config(site1)) {
          diag_corr.push_back(0.0);
        } else {
          TenElemT psi_ex = tn.ReplaceOneSiteTrace(site2, (*split_index_tps)(site2)[1], HORIZONTAL);
          diag_corr.push_back(psi_ex * inv_psi);
        }
      }
      tn(site1) = (*split_index_tps)(site1)[config(site1)]; // change back

      if (config(site1) == 1) {
        for (size_t i = 1; i <= lx / 2; i++) {  //sp(i) * sm(j) = 0
          res.two_point_functions_loc.push_back(0.0);
        }
        res.two_point_functions_loc.insert(res.two_point_functions_loc.end(), diag_corr.begin(), diag_corr.end());
      } else {
        res.two_point_functions_loc.insert(res.two_point_functions_loc.end(), diag_corr.begin(), diag_corr.end());
        for (size_t i = 1; i <= lx / 2; i++) {  //sm(i) * sp(j) = 0
          res.two_point_functions_loc.push_back(0.0);
        }
      }
    }
    if (row < tn.rows() - 1) {
      tn.BMPSMoveStep(DOWN, trunc_para);
    }
  }

  //Calculate vertical bond energy contribution
  tn.GenerateBMPSApproach(LEFT, trunc_para);
  for (size_t col = 0; col < tn.cols(); col++) {
    tn.InitBTen(UP, col);
    tn.GrowFullBTen(DOWN, col, 2, true);
    tps_sample->amplitude = tn.Trace({0, col}, VERTICAL);
    inv_psi = 1.0 / tps_sample->amplitude;
    for (size_t row = 0; row < tn.rows() - 1; row++) {
      const SiteIdx site1 = {row, col};
      const SiteIdx site2 = {row + 1, col};
      double vertical_bond_energy;
      if (config(site1) == config(site2)) {
        vertical_bond_energy = 0.25;
      } else {
        TenElemT psi_ex = tn.ReplaceNNSiteTrace(site1, site2, VERTICAL,
                                                (*split_index_tps)(site1)[config(site2)],
                                                (*split_index_tps)(site2)[config(site1)]);
        TenElemT ratio = psi_ex * inv_psi;
        if (std::abs(ratio) > bond_energy_extremly_large) {
          std::cout << "Warning for possible numeric floating point err: \n"
                    << "Site : (" << row << ", " << col << ") "
                    << "Bond Orientation :" << "Vertical"
                    << "psi_exchange : " << std::scientific << psi_ex
                    << ", psi_0 : " << std::scientific << tps_sample->amplitude
                    << ", ratio : " << std::scientific << ratio
                    << "original tensor norms : (" << tn(site1).Norm2() << ", " << tn(site2).Norm2() << ") "
                    << "exchange tensor norms : (" << (*split_index_tps)(site1)[config(site2)].Norm2()
                    << ", " << (*split_index_tps)(site2)[config(site1)].Norm2() << ") "
                    << std::endl;
          // set the bond energy = 0.0
        } else {
          vertical_bond_energy = (-0.25 + ratio * 0.5);
        }
      }
      energy += vertical_bond_energy;
      res.bond_energys_loc.push_back(vertical_bond_energy);
      if (row < tn.rows() - 2) {
        tn.BTenMoveStep(DOWN);
      }
    }
    if (col < tn.cols() - 1) {
      tn.BMPSMoveStep(RIGHT, trunc_para);
    }
  }
  res.energy_loc = energy;
  res.one_point_functions_loc.reserve(tn.rows() * tn.cols());
  for (auto &spin_config : config) {
    res.one_point_functions_loc.push_back((double) spin_config - 0.5);
  }
  return res;
}

}//gqpeps




#endif //GRACEQ_VMC_PEPS_SPIN_ONEHALF_HEISENBERG_SQUARE_H
