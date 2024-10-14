// SPDX-License-Identifier: LGPL-3.0-only

/*
* Author: Hao-Xin Wang<wanghaoxin1996@gmail.com>
* Creation Date: 2024-08-16
*
* Description: QuantumLiquids/PEPS project. Unittests for PEPS Simple Update in fermion model.
*/

#define PLAIN_TRANSPOSE 1

#include "gtest/gtest.h"
#include "qlten/qlten.h"
#include "qlmps/case_params_parser.h"
#include "qlpeps/algorithm/simple_update/simple_update_model_all.h"
#include "qlpeps/algorithm/loop_update/loop_update.h"

using namespace qlten;
using namespace qlpeps;

using qlmps::CaseParamsParserBasic;
char *params_file;

using qlten::special_qn::fZ2QN;

struct SimpleUpdateTestParams : public CaseParamsParserBasic {
  SimpleUpdateTestParams(const char *f) : CaseParamsParserBasic(f) {
    Lx = ParseInt("Lx");
    Ly = ParseInt("Ly");
    D = ParseInt("D");
    Tau0 = ParseDouble("Tau0");
    Steps = ParseInt("Steps");
  }

  size_t Ly;
  size_t Lx;
  size_t D;
  double Tau0;
  size_t Steps;
};

struct Z2SpinlessFreeFermionTools : public testing::Test {
  using IndexT = Index<fZ2QN>;
  using QNSctT = QNSector<fZ2QN>;
  using QNSctVecT = QNSectorVec<fZ2QN>;

  using DTensor = QLTensor<QLTEN_Double, fZ2QN>;
  SimpleUpdateTestParams params = SimpleUpdateTestParams(params_file);
  size_t Lx = params.Lx; //cols
  size_t Ly = params.Ly;

  double t = 1.0;
  fZ2QN qn0 = fZ2QN(0);
  // |ket>
  IndexT loc_phy_ket = IndexT({QNSctT(fZ2QN(1), 1),  // |1> occupied
                               QNSctT(fZ2QN(0), 1)}, // |0> empty state
                              TenIndexDirType::IN
  );
  // <bra|
  IndexT loc_phy_bra = InverseIndex(loc_phy_ket);

  DTensor c = DTensor({loc_phy_ket, loc_phy_bra});   // annihilation operator
  DTensor cdag = DTensor({loc_phy_ket, loc_phy_bra});// creation operator
  DTensor n = DTensor({loc_phy_ket, loc_phy_bra});   // density operator

  DTensor ham_nn = DTensor({loc_phy_ket, loc_phy_ket, loc_phy_bra, loc_phy_bra});//site: i-j-j-i (i<j)
  void SetUp(void) {
    n({0, 0}) = 1.0;
    c({1, 0}) = 1;
    cdag({0, 1}) = 1;

    ham_nn({1, 0, 1, 0}) = -t;
    ham_nn({0, 1, 0, 1}) = -t;
    ham_nn.Transpose({3, 0, 2, 1}); // transpose indices order for consistent with simple update convention
  }
};

double CalGroundStateEnergyForSpinlessNNFreeFermionOBC(
    const size_t Lx,
    const size_t Ly,
    const size_t particle_num
) {
  const size_t num_sites = Lx * Ly;
  std::vector<double> energy_levels;

  // Calculate the energy levels
  for (size_t kx = 0; kx < Lx; ++kx) {
    for (size_t ky = 0; ky < Ly; ++ky) {
      double theta_x = M_PI * (kx + 1) / (Lx + 1);
      double theta_y = M_PI * (ky + 1) / (Ly + 1);
      double energy = -2 * (std::cos(theta_x) + std::cos(theta_y));
      energy_levels.push_back(energy);
    }
  }

  // Sort energy levels in ascending order
  std::sort(energy_levels.begin(), energy_levels.end());

  // Sum the lowest `particle_num` energy levels
  double ground_state_energy = 0.0;
  for (size_t i = 0; i < particle_num; ++i) {
    ground_state_energy += energy_levels[i];
  }

  return ground_state_energy;
}

TEST_F(Z2SpinlessFreeFermionTools, HalfFillingSimpleUpdate) {
  qlten::hp_numeric::SetTensorManipulationThreads(1);
  SquareLatticePEPS<QLTEN_Double, fZ2QN> peps0(loc_phy_ket, Ly, Lx);

  std::vector<std::vector<size_t>> activates(Ly, std::vector<size_t>(Lx));
  //half-filling
  size_t n_int = 0;
  for (size_t y = 0; y < Ly; y++) {
    for (size_t x = 0; x < Lx; x++) {
      activates[y][x] = n_int % 2;
      n_int++;
    }
  }
  peps0.Initial(activates);

  SimpleUpdatePara update_para(params.Steps, params.Tau0, 1, params.D, 1e-10);
  SimpleUpdateExecutor<QLTEN_Double, fZ2QN>
      *su_exe = new SquareLatticeNNSimpleUpdateExecutor<QLTEN_Double, fZ2QN>(update_para, peps0,
                                                                             ham_nn);
  su_exe->Execute();
  auto peps = su_exe->GetPEPS();
  delete su_exe;
  peps.Dump("peps_spinless_free_fermion_half_filling");
  double exact_gs_energy = CalGroundStateEnergyForSpinlessNNFreeFermionOBC(Lx, Ly, Lx * Ly / 2);
  std::cout << "Exact ground state energy : " << exact_gs_energy << std::endl;
}

struct Z2tJModelTools : public testing::Test {
  using IndexT = Index<fZ2QN>;
  using QNSctT = QNSector<fZ2QN>;
  using QNSctVecT = QNSectorVec<fZ2QN>;

  using DTensor = QLTensor<QLTEN_Double, fZ2QN>;
  using ZTensor = QLTensor<QLTEN_Complex, fZ2QN>;

  SimpleUpdateTestParams params = SimpleUpdateTestParams(params_file);
  size_t Lx = params.Lx; //cols
  size_t Ly = params.Ly;
  double t = 1.0;
  double J = 0.3;
  double doping = 0.125;
  fZ2QN qn0 = fZ2QN(0);
  IndexT loc_phy_ket = IndexT({QNSctT(fZ2QN(1), 2), // |up>, |down>
                               QNSctT(fZ2QN(0), 1)}, // |0> empty state
                              TenIndexDirType::IN
  );
  IndexT loc_phy_bra = InverseIndex(loc_phy_ket);

  // nearest-neighbor Hamiltonian term, for the construction of evolve gates
  DTensor dham_tj_nn = DTensor({loc_phy_ket, loc_phy_ket, loc_phy_bra, loc_phy_bra}); //i-j-j-i (i < j)
  // loop update data
  IndexT vb_out = IndexT({QNSctT(fZ2QN(0), 4),
                          QNSctT(fZ2QN(1), 4)},
                         TenIndexDirType::OUT
  );
  IndexT vb_in = InverseIndex(vb_out);

  double loop_tau = 0.01;
  using LoopGateT = LoopGates<DTensor>;
  DuoMatrix<LoopGateT> evolve_gates = DuoMatrix<LoopGateT>(Ly - 1, Lx - 1);

  void SetUp(void) {
    //set up nearest-neighbor Hamiltonian terms
    //-t (c^dag_{i, s} c_{j,s} + c^dag_{j,s} c_{i,s}) + J S_i \cdot S_j
    dham_tj_nn({2, 0, 2, 0}) = -t;
    dham_tj_nn({2, 1, 2, 1}) = -t;
    dham_tj_nn({0, 2, 0, 2}) = -t;
    dham_tj_nn({1, 2, 1, 2}) = -t;

    dham_tj_nn({0, 0, 0, 0}) = 0.25 * J;    //FM, diagonal element
    dham_tj_nn({1, 1, 1, 1}) = 0.25 * J;    //FM, diagonal element
    dham_tj_nn({0, 1, 1, 0}) = -0.25 * J;   //AFM,diagonal element
    dham_tj_nn({1, 0, 0, 1}) = -0.25 * J;   //AFM,diagonal element
    dham_tj_nn({0, 1, 0, 1}) = 0.5 * J;     //off diagonal element
    dham_tj_nn({1, 0, 1, 0}) = 0.5 * J;     //off diagonal element

    dham_tj_nn.Transpose({3, 0, 2, 1});
    GenerateSquaretJAllEvolveGates(loop_tau, evolve_gates);
  }

  void GenerateSquaretJAllEvolveGates(
      const double tau, // imaginary time
      DuoMatrix<LoopGateT> &evolve_gates //output
  ) {
    //corner
    evolve_gates({0, 0}) = GenerateSquaretJLoopGates(tau,
                                                     1, 2, 2, 1);
    evolve_gates({0, Lx - 2}) = GenerateSquaretJLoopGates(tau,
                                                          1, 1, 2, 2);
    evolve_gates({Ly - 2, 0}) = GenerateSquaretJLoopGates(tau,
                                                          2, 2, 1, 1);
    evolve_gates({Ly - 2, Lx - 2}) = GenerateSquaretJLoopGates(tau,
                                                               2, 1, 1, 2);

    auto gates_upper = GenerateSquaretJLoopGates(tau,
                                                 1, 2, 2, 2);
    auto gates_lower = GenerateSquaretJLoopGates(tau,
                                                 2, 2, 1, 2);
    for (size_t col = 1; col < Lx - 2; col++) {
      evolve_gates({0, col}) = gates_upper;
      evolve_gates({Ly - 2, col}) = gates_lower;
    }

    auto gates_left = GenerateSquaretJLoopGates(tau,
                                                2, 2, 2, 1);
    auto gates_middle = GenerateSquaretJLoopGates(tau,
                                                  2, 2, 2, 2);
    auto gates_right = GenerateSquaretJLoopGates(tau,
                                                 2, 1, 2, 2);
    for (size_t row = 1; row < Ly - 2; row++) {
      evolve_gates({row, 0}) = gates_left;
      evolve_gates({row, Lx - 2}) = gates_right;
    }
    for (size_t col = 1; col < Lx - 2; col++) {
      for (size_t row = 1; row < Ly - 2; row++) {
        evolve_gates({row, col}) = gates_middle;
      }
    }
  }

  LoopGateT GenerateSquaretJLoopGates(
      const double tau, // imaginary time
      const size_t n0, const size_t n1, const size_t n2, const size_t n3  //bond share number
      //n0 bond is the upper horizontal bond in the loop
  ) {
    const std::vector<size_t> ns = {n0, n1, n2, n3};
    LoopGateT gates;
    for (size_t i = 0; i < 4; i++) {

      gates[i] = DTensor({vb_in, loc_phy_bra, loc_phy_ket, vb_out});
      DTensor &gate = gates[i];
      //Id
      gate({0, 0, 0, 0}) = 1.0;
      gate({0, 1, 1, 0}) = 1.0;
      gate({0, 2, 2, 0}) = 1.0;
      //-s_z * tau
      gate({0, 0, 0, 1}) = -0.5 * tau * J / double(ns[i]);
      gate({0, 1, 1, 1}) = 0.5 * tau * J / double(ns[i]);
      //s_z
      gate({1, 0, 0, 0}) = 0.5;
      gate({1, 1, 1, 0}) = -0.5;

      //-s^+ * tau/2
      gate({0, 0, 1, 2}) = -1.0 * tau * J / double(ns[i]) / 2.0;
      //s^-
      gate({2, 1, 0, 0}) = 1.0;

      //-s^- * tau/2
      gate({0, 1, 0, 3}) = -1.0 * tau * J / double(ns[i]) / 2.0;
      //s^+
      gate({3, 0, 1, 0}) = 1.0;

      gate({0, 2, 0, 4}) = (-tau) * (-t) / double(ns[i]) * (-1);
      gate({4, 0, 2, 0}) = 1.0;

      gate({0, 2, 1, 5}) = (-tau) * (-t) / double(ns[i]) * (-1);
      gate({5, 1, 2, 0}) = 1.0;

      gate({0, 0, 2, 6}) = (-tau) * (-t) / double(ns[i]);
      gate({6, 2, 0, 0}) = 1.0;

      gate({0, 1, 2, 7}) = (-tau) * (-t) / double(ns[i]);
      gate({7, 2, 1, 0}) = 1.0;
      gate.Div();
    }
    return gates;

  }
};

TEST_F(Z2tJModelTools, tJModelHalfFillingSimpleUpdate) {
  // ED ground state energy in 4x4 = -9.189207065192949 * J
  qlten::hp_numeric::SetTensorManipulationThreads(1);
  SquareLatticePEPS<QLTEN_Double, fZ2QN> peps0(loc_phy_ket, Ly, Lx);

  std::vector<std::vector<size_t>> activates(Ly, std::vector<size_t>(Lx));
  //half-filling
  for (size_t y = 0; y < Ly; y++) {
    for (size_t x = 0; x < Lx; x++) {
      size_t sz_int = x + y;
      activates[y][x] = sz_int % 2;
    }
  }
  peps0.Initial(activates);

  SimpleUpdatePara update_para(params.Steps, params.Tau0, 1, params.D, 1e-10);
  SimpleUpdateExecutor<QLTEN_Double, fZ2QN>
      *su_exe = new SquareLatticeNNSimpleUpdateExecutor<QLTEN_Double, fZ2QN>(update_para, peps0,
                                                                             dham_tj_nn);
  su_exe->Execute();
  auto peps = su_exe->GetPEPS();
  delete su_exe;
  for (auto gamma : peps.Gamma) {
    EXPECT_EQ(gamma.GetQNBlkNum(), 1);
  }
  for (auto lam : peps.lambda_horiz) {
    EXPECT_EQ(lam.GetQNBlkNum(), 1);
  }
  for (auto lam : peps.lambda_vert) {
    EXPECT_EQ(lam.GetQNBlkNum(), 1);
  }
}

TEST_F(Z2tJModelTools, tJModelDopingSimpleUpdate) {
  // ED ground state energy in 4x4 -6.65535490684301
  qlten::hp_numeric::SetTensorManipulationThreads(1);
  SquareLatticePEPS<QLTEN_Double, fZ2QN> peps0(loc_phy_ket, Ly, Lx);
  std::string peps_path = "peps_tj_doping0.125";
  if (IsPathExist(peps_path)) {
    peps0.Load(peps_path);
  } else {
    std::vector<std::vector<size_t>> activates(Ly, std::vector<size_t>(Lx));
    size_t site_idx = 0, sz_int = 0;
    size_t sites_per_hole = (size_t) (1.0 / doping);
    for (size_t y = 0; y < Ly; y++) {
      for (size_t x = 0; x < Lx; x++) {
        if (site_idx % sites_per_hole == 1) {
          activates[y][x] = 2;
        } else {
          activates[y][x] = sz_int % 2;
          sz_int++;
        }
        site_idx++;
      }
    }
    peps0.Initial(activates);
  }

  SimpleUpdatePara update_para(params.Steps, params.Tau0, 1, params.D, 1e-10);
  SimpleUpdateExecutor<QLTEN_Double, fZ2QN>
      *su_exe = new SquareLatticeNNSimpleUpdateExecutor<QLTEN_Double, fZ2QN>(update_para, peps0,
                                                                             dham_tj_nn);
  su_exe->Execute();
  auto peps = su_exe->GetPEPS();
  delete su_exe;
  peps.Dump(peps_path);
}

TEST_F(Z2tJModelTools, tJModelDopingLoopUpdate) {
  qlten::hp_numeric::SetTensorManipulationThreads(1);
  omp_set_num_threads(1);
  SquareLatticePEPS<QLTEN_Double, fZ2QN> peps0(loc_phy_ket, Ly, Lx);
  peps0.Load("peps_tj_doping0.125");
  peps0.NormalizeAllTensor();

  ArnoldiParams arnoldi_params(1e-10, 100);
  double fet_tol = 1e-13;
  double fet_max_iter = 30;
  ConjugateGradientParams cg_params(200, 1e-6, 20, 0.0);

  FullEnvironmentTruncateParams fet_params(1, 4, 1e-10,
                                           fet_tol, fet_max_iter,
                                           cg_params);

  auto *loop_exe = new LoopUpdateExecutor<QLTEN_Double, fZ2QN>(LoopUpdateTruncatePara(
                                                                   arnoldi_params,
                                                                   1e-6,
                                                                   fet_params),
                                                               150,
                                                               loop_tau,
                                                               evolve_gates,
                                                               peps0);

  loop_exe->Execute();
  auto peps = loop_exe->GetPEPS();
  delete loop_exe;
  peps.Dump("peps_tj_doping0.125");
}

int main(int argc, char *argv[]) {
  testing::InitGoogleTest(&argc, argv);
  std::cout << argc << std::endl;
  params_file = argv[1];
  return RUN_ALL_TESTS();
}