
/*
* Copyright (c) 2017-2022 Nils Kohl.
*
* This file is part of HyTeG
* (see https://i10git.cs.fau.de/hyteg/hyteg).
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#ifndef HYTEG_BUILD_WITH_PETSC
WALBERLA_ABORT( "This test only works with PETSc enabled. Please enable it via -DHYTEG_BUILD_WITH_PETSC=ON" )
#endif

#include <fstream>
#include <hyteg/petsc/PETScBlockPreconditionedStokesSolver.hpp>
#include <limits>

#include "hyteg/MeshQuality.hpp"
#include "hyteg/composites/P1DGEP0StokesFunction.hpp"
#include "hyteg/composites/P1DGEP0StokesOperator.hpp"
#include "hyteg/composites/P1P0StokesFunction.hpp"
#include "hyteg/composites/P1P0StokesOperator.hpp"
#include "hyteg/composites/P2P1TaylorHoodFunction.hpp"
#include "hyteg/dataexport/VTKOutput/VTKOutput.hpp"
#include "hyteg/elementwiseoperators/P2P1ElementwiseAffineEpsilonStokesOperator.hpp"
#include "hyteg/functions/FunctionTraits.hpp"
#include "hyteg/gridtransferoperators/P1toP1LinearProlongation.hpp"
#include "hyteg/gridtransferoperators/P2toP2QuadraticProlongation.hpp"
#include "hyteg/mesh/MeshInfo.hpp"
#include "hyteg/p0functionspace/P0P0MassForm.hpp"
#include "hyteg/petsc/PETScCGSolver.hpp"
#include "hyteg/petsc/PETScLUSolver.hpp"
#include "hyteg/petsc/PETScManager.hpp"
#include "hyteg/petsc/PETScMinResSolver.hpp"
#include "hyteg/petsc/PETScSparseMatrix.hpp"
#include "hyteg/primitivestorage/SetupPrimitiveStorage.hpp"
#include "hyteg/solvers/CGSolver.hpp"
#include "hyteg/solvers/MinresSolver.hpp"
#include "hyteg/solvers/solvertemplates/StokesSolverTemplates.hpp"

#include "EGOperators.hpp"
#include "EGOperatorsNitscheBC.hpp"
#include "constant_stencil_operator/P1ConstantOperator.hpp"
#include "constant_stencil_operator/P2ConstantOperator.hpp"
#include "mixed_operator/P2P1TaylorHoodStokesOperator.hpp"

namespace hyteg {
    namespace dg {
        namespace eg {
            auto copyBdry = [](EGP0StokesFunction<real_t> fun) {
                fun.p().setBoundaryCondition(fun.uvw().getBoundaryCondition());
            };

// scalar lambda for one component of analytical solution and rhs
            typedef std::function<real_t(const hyteg::PointND<real_t, 3> &p)> ScalarLambda;

// tuple of function for solution (u,p) and rhs of vector values stokes equation
            typedef std::tuple<ScalarLambda, ScalarLambda, ScalarLambda, ScalarLambda> LambdaTuple;

// datatype for errors and rates
            typedef std::vector<real_t> ErrorArray;

// checks for discretization and operator template arguments
            template<typename StokesOperatorType>
            constexpr bool isEGP0Discr() {
                return std::is_same<StokesOperatorType, EGP0EpsilonStokesOperator>::value ||
                       std::is_same<StokesOperatorType, EGP0StokesOperator>::value ||
                       std::is_same<StokesOperatorType, EGP0StokesOperatorNitscheBC>::value ||
                       std::is_same<StokesOperatorType, EGP0IIPGStokesOperator>::value ||
                       std::is_same<StokesOperatorType, EGP0EpsilonOperatorStokesNitscheBC>::value;
            }

            template<typename StokesOperatorType>
            constexpr bool usesNitscheBCs() {
                return std::is_same<StokesOperatorType, EGP0StokesOperatorNitscheBC>::value ||
                       std::is_same<StokesOperatorType, EGP0EpsilonOperatorStokesNitscheBC>::value;
            }

            template<typename StokesOperatorType>
            constexpr bool isEpsilonOp() {
                return std::is_same<StokesOperatorType, EGP0EpsilonStokesOperator>::value ||
                       std::is_same<StokesOperatorType, EGEpsilonOperatorNitscheBC>::value ||
                       std::is_same<StokesOperatorType, EGP0EpsilonOperatorStokesNitscheBC>::value ||
                       std::is_same<StokesOperatorType, hyteg::P2P1ElementwiseAffineEpsilonStokesOperator>::value;
            }

            template<typename StokesOperatorType>
            constexpr bool isP2P1Discr() {
                return std::is_same<StokesOperatorType, hyteg::P2P1ElementwiseAffineEpsilonStokesOperator>::value ||
                       std::is_same<StokesOperatorType, hyteg::P2P1TaylorHoodStokesOperator>::value;
            }

            template<typename StokesOperatorType>
            constexpr bool isP1P0Discr() {
                return std::is_same<StokesOperatorType, hyteg::P1P0StokesOperator>::value;
            }

// check for data member energyOp
            template<typename, typename = void>
            constexpr bool hasEnergyNormOp = false;
            template<typename T>
            constexpr bool hasEnergyNormOp<T, std::void_t<decltype(std::declval<T>().energyNormOp)> > = true;

            template<typename StokesOperatorType>
            class StokesConvergenceOrderTest {
            public:
                using StokesFunctionType = typename StokesOperatorType::srcType;
                using StokesFunctionNumeratorType = typename StokesFunctionType::template FunctionType<idx_t>;
                // datatype for check function on the numerical solution
                typedef std::function<void(const StokesFunctionType &, real_t h)> checkFunctionType;

                StokesConvergenceOrderTest( const std::string&                           testName,
                                            LambdaTuple                                  solTuple,
                                            LambdaTuple                                  rhsTuple,
                                            const std::shared_ptr< StokesOperatorType >& Op,
                                            const std::shared_ptr< PrimitiveStorage >&   storage,
                                            const uint_t                                 minLevel,
                                            const uint_t                                 maxLevel,
                                            const uint_t&                                solverType  = 5,
                                            const real_t                                 residualTol = 1e-7,
                                            bool                                         writeVTK    = false,
                                            std::pair< bool, real_t > discrErrorThreshhold           = std::make_pair( false, 0 ),
                                            std::shared_ptr< checkFunctionType > checkSolution       = nullptr,
                                            real_t                               expectedL2VeloRate  = 2.0 )
                : testName_( testName )
                , maxLevel_( maxLevel )
                , solTuple_( solTuple )
                , rhsTuple_( rhsTuple )
                , Op_( Op )
                , storage_( storage )
                , solverType_( solverType )
                , writeVTK_( writeVTK )
                , residualTol_( residualTol )
                , checkSolution_( checkSolution )
                , discrErrorThreshhold_( discrErrorThreshhold )
                {
                   WALBERLA_LOG_INFO_ON_ROOT( "Running " << testName );
                   auto ratesCounter = EGConvRatesCounter( expectedL2VeloRate );
                   ratesCounter.printHeader();

                   for ( uint_t level = minLevel; level <= maxLevel; level++ )
                   {
                      ratesCounter.update(
                          RunStokesTestOnLevel( level ), MeshQuality::getMaximalEdgeLength( storage_, level ), testName );
                      ratesCounter.printCurrentRates( level );
                      if ( level > minLevel )
                         ratesCounter.checkL2VeloRate( level );
                   }
                   ratesCounter.printMeanRates();
                }

              private:
                // subclass handling the computation of convergence rates
                class EGConvRatesCounter {
                public:
                   EGConvRatesCounter( real_t expectedL2VeloRate )
                   : nUpdates_( 0 )
                   , h_old( std::numeric_limits< real_t >::max() )
                   , expectedL2VeloRate_( expectedL2VeloRate )
                   {
                      errors_   = { 0., 0., 0. };
                      rates_    = { 0., 0., 0. };
                      sumRates_ = { 0., 0., 0. };
                   }

                   void update( const ErrorArray& newErrors, real_t h_new, const std::string& fname )
                   {
                      currentDoFs_ = newErrors[0];
                      std::transform(newErrors.begin() + 1, newErrors.end(), errors_.begin(), rates_.begin(),
                                       std::divides<real_t>());
                        std::transform(rates_.begin(), rates_.end(), rates_.begin(), [h_new, this](real_t x) {
                            return std::log(x) / std::log(h_new / h_old);
                        });
                        if (nUpdates_ > 0)
                            std::transform(rates_.begin(), rates_.end(), sumRates_.begin(), sumRates_.begin(),
                                           std::plus<real_t>());
                        nUpdates_++;
                        h_old = h_new;
                        errors_.assign(newErrors.begin() + 1, newErrors.end());

                        writeErrors(h_new, errors_[0], errors_[2], fname);
                    }

                    void writeErrors(real_t h_new, real_t l2VelError, real_t l2pError, const std::string &fname) {
                        std::ofstream err_file;
                        auto fpath = "/mnt/c/Users/Fabia/OneDrive/Desktop/hyteg-plots/EG_ConvOrders/" + fname + ".txt";
                        err_file.open(fpath, std::ios_base::app);
                        err_file << h_new << ", " << l2VelError << ", " << l2pError << "\n";
                        err_file.close();
                    }

                    void printHeader() {
                        WALBERLA_LOG_INFO_ON_ROOT(walberla::format("%6s|%15s|%15s|%15s|%15s|%15s|%15s|%15s|",
                                                                   "level",
                                                                   "DoFs",
                                                                   "L2Norm(e_v)",
                                                                   "ENorm(e_v)",
                                                                   "L2Norm(e_p)",
                                                                   "L2Rate_v",
                                                                   "ERate_v",
                                                                   "L2rate_p"));
                    }

                    void printMeanRates() {
                        WALBERLA_LOG_INFO_ON_ROOT(
                                walberla::format("%15s|%15.2s|%15.2s|%15.2s|", "", "L2Rate_v", "ERate_v", "L2rate_p"));
                        WALBERLA_LOG_INFO_ON_ROOT(walberla::format("%15s|%15.2e|%15.2e|%15.2e|",
                                                                   "Mean rates:",
                                                                   sumRates_[0] / real_c(nUpdates_ - 1),
                                                                   sumRates_[1] / real_c(nUpdates_ - 1),
                                                                   sumRates_[2] / real_c(nUpdates_ - 1)));
                    }

                    void printCurrentRates(uint_t level) {
                        WALBERLA_LOG_INFO_ON_ROOT(
                                walberla::format("%6d|%15.2e|%15.2e|%15.2e|%15.2e|%15.2e|%15.2e|%15.2e|",
                                                 level,
                                                 currentDoFs_,
                                                 errors_[0],
                                                 errors_[1],
                                                 errors_[2],
                                                 rates_[0],
                                                 rates_[1],
                                                 rates_[2]));
                    }

                    void checkL2VeloRate(uint_t level) {
                        WALBERLA_LOG_INFO_ON_ROOT(
                                "Convergence L2 rate on level " << level << ": " << rates_[0] << ", expected rate: ["
                                                                << expectedL2VeloRate_ - 0.1 << ", "
                                                                << expectedL2VeloRate_ + 0.1 << "]");
                        /* WALBERLA_CHECK_LESS_EQUAL(rates_[0],
                                                                 expectedL2VeloRate_ + 0.1,
                                                                 "Convergence L2 rate on level " << level << " too large (computed: "
                                                                                                 << rates_[0]
                                                                                                 << ", expected + eps: "
                                                                                                 << expectedL2VeloRate_ + 0.1 << ")");*/
                        WALBERLA_CHECK_GREATER_EQUAL(rates_[0],
                                                     expectedL2VeloRate_ - 0.25,
                                                     "Convergence L2 rate on level " << level
                                                                                     << " too small (computed: "
                                                                                     << rates_[0]
                                                                                     << ", expected - eps: "
                                                                                     << expectedL2VeloRate_ - 0.25
                                                                                     << ")");
                    }

                private:
                    // e_v e_v_conf e_v_disc e_p
                    ErrorArray errors_;
                    ErrorArray rates_;
                    ErrorArray sumRates_;
                    int nUpdates_;
                    real_t h_old;
                    real_t currentDoFs_;
                    real_t expectedL2VeloRate_;
                };

                // subclass handling the computation of error norms
                class EGNormComputer {
                public:
                    EGNormComputer(uint_t level, StokesFunctionType &err,
                                   const std::shared_ptr<PrimitiveStorage> &storage)
                            : level_(level), storage_(storage), err_(err),
                              tmpErr_("tmpErr", storage, level, level + 1) {}

                    ErrorArray compute(typename StokesOperatorType::EnergyNormOperator_T &energyNormOp) {
                        return {L2VeloError(), EnergyVeloError(energyNormOp), L2PressureError()};
                    }

                    real_t L2PressureError() {
                        if constexpr (isEGP0Discr<StokesOperatorType>() || isP1P0Discr<StokesOperatorType>()) {
                            auto mass_form = std::make_shared<dg::P0P0MassForm>();
                            dg::DGOperator M_pressure(storage_, level_, level_, mass_form);
                            M_pressure.apply(*err_.p().getDGFunction(), *tmpErr_.p().getDGFunction(), level_, All,
                                             Replace);
                        } else if constexpr (isP2P1Discr<StokesOperatorType>()) {
                            P1ConstantMassOperator M_pressure(storage_, level_, level_);
                            M_pressure.apply(err_.p(), tmpErr_.p(), level_, All, Replace);
                        } else {
                            WALBERLA_ABORT("Not implemented.");
                        }
                        return sqrt(err_.p().dotGlobal(tmpErr_.p(), level_, Inner));
                    }

                    real_t EnergyVeloError(typename StokesOperatorType::EnergyNormOperator_T &energyNormOp) {
                        energyNormOp.apply(err_.uvw(), tmpErr_.uvw(), level_, Inner, Replace);
                        return sqrt(err_.uvw().dotGlobal(tmpErr_.uvw(), level_, All));
                    }

                    real_t L2VeloError() {
                        if constexpr (isEGP0Discr<StokesOperatorType>()) {
                            EGMassOperator M_vel(storage_, level_, level_ + 1);
                            M_vel.apply(err_.uvw(), tmpErr_.uvw(), level_, All, Replace);
                            return sqrt(err_.uvw().dotGlobal(tmpErr_.uvw(), level_, All));
                            /* real_t e_v_disc = sqrt(
                                                    err_.uvw().dotGlobal(err_.uvw(), level_, All) /
                                                    real_c(numberOfGlobalDoFs(err_.uvw(), level_)));
                                            return e_v_disc;*/
                        } else if constexpr (isP1P0Discr<StokesOperatorType>()) {
                            P1ConstantVectorMassOperator M_vel(storage_, level_, level_);
                            M_vel.apply(err_.uvw(), tmpErr_.uvw(), level_, All, Replace);
                        } else if constexpr (isP2P1Discr<StokesOperatorType>()) {
                            P2ConstantMassOperator M_vel(storage_, level_, level_ + 1);
                            if (!storage_->hasGlobalCells()) {
                                M_vel.apply(err_.uvw()[0], tmpErr_.uvw()[0], level_ + 1, Inner, Replace);
                                M_vel.apply(err_.uvw()[1], tmpErr_.uvw()[1], level_ + 1, Inner, Replace);
                            } else {
                                M_vel.apply(err_.uvw()[0], tmpErr_.uvw()[0], level_ + 1, Inner, Replace);
                                M_vel.apply(err_.uvw()[1], tmpErr_.uvw()[1], level_ + 1, Inner, Replace);
                                M_vel.apply(err_.uvw()[2], tmpErr_.uvw()[2], level_ + 1, Inner, Replace);
                            }
                            return sqrt(err_.uvw().dotGlobal(tmpErr_.uvw(), level_ + 1, All));
                        } else {
                            WALBERLA_ABORT("Not implemented.");
                        }
                        return sqrt(err_.uvw().dotGlobal(tmpErr_.uvw(), level_, All));
                    }

                    const uint_t level_;
                    const std::shared_ptr<PrimitiveStorage> &storage_;
                    StokesFunctionType &err_;
                    StokesFunctionType tmpErr_;
                };

                void setupRHSinexact(uint_t level, const StokesFunctionType &f, StokesFunctionType &rhs) {
                    // solution, rhs as a lambda function
                    auto [f_x_expr, f_y_expr, f_z_expr, g_expr] = rhsTuple_;

                    // setup the rhs linear form inexactly by just multiplying the rhs function by the massmatrix corresponding to the discretization
                    if constexpr (isP2P1Discr<StokesOperatorType>()) {
                        P2ConstantMassOperator M_vel(storage_, level, level);
                        if (!storage_->hasGlobalCells()) {
                            M_vel.apply(f.uvw()[0], rhs.uvw()[0], level, All, Replace);
                            M_vel.apply(f.uvw()[1], rhs.uvw()[1], level, All, Replace);
                        } else {
                            M_vel.apply(f.uvw()[0], rhs.uvw()[0], level, All, Replace);
                            M_vel.apply(f.uvw()[1], rhs.uvw()[1], level, All, Replace);
                            M_vel.apply(f.uvw()[2], rhs.uvw()[2], level, All, Replace);
                        }
                        P1ConstantMassOperator M_pressure(storage_, level, level);
                        M_pressure.apply(f.p(), rhs.p(), level, All, Replace);
                    } else if constexpr (isEGP0Discr<StokesOperatorType>()) {
                        EGMassOperator M_vel(storage_, level, level);
                        M_vel.apply(f.uvw(), rhs.uvw(), level, All, Replace);
                        auto mass_form = std::make_shared<dg::P0P0MassForm>();
                        dg::DGOperator M_pressure(storage_, level, level, mass_form);
                        M_pressure.apply(*f.p().getDGFunction(), *rhs.p().getDGFunction(), level, All, Replace);
                    } else if constexpr (isP1P0Discr<StokesOperatorType>()) {
                        P1ConstantVectorMassOperator M_vel(storage_, level, level);
                        M_vel.apply(f.uvw(), rhs.uvw(), level, All, Replace);
                        auto mass_form = std::make_shared<dg::P0P0MassForm>();
                        dg::DGOperator M_pressure(storage_, level, level, mass_form);
                        M_pressure.apply(*f.p().getDGFunction(), *rhs.p().getDGFunction(), level, All, Replace);
                    } else {
                        WALBERLA_ABORT("Benchmark not implemented for other discretizations!");
                    }
                }

                // apply bcs to the solution function
                void setupBC(uint_t level, const StokesFunctionType &u) {
                    auto [u_x_expr, u_y_expr, u_z_expr, p_expr] = solTuple_;
                    if constexpr (isP2P1Discr<StokesOperatorType>() || isP1P0Discr<StokesOperatorType>()) {
                        if (!storage_->hasGlobalCells()) {
                            u.uvw().interpolate({u_x_expr, u_y_expr}, level, DirichletBoundary);
                        } else {
                            u.uvw().interpolate({u_x_expr, u_y_expr, u_z_expr}, level, DirichletBoundary);
                        }
                    } else if constexpr (isEGP0Discr<StokesOperatorType>()) {
                        if (!storage_->hasGlobalCells()) {
                            u.uvw().getConformingPart()->interpolate({u_x_expr, u_y_expr}, level, DirichletBoundary);
                        } else {
                            u.uvw().getConformingPart()->interpolate({u_x_expr, u_y_expr, u_z_expr}, level,
                                                                     DirichletBoundary);
                        }
                    }
                }

                // integrate the rhs directly and apply the boundary values corresponding to the used operator to the rhs
                void integrateRHS(uint_t level, StokesFunctionType &rhs) {
                    static_assert(std::is_same<StokesOperatorType, EGP0StokesOperatorNitscheBC>::value ||
                                  std::is_same<StokesOperatorType, EGP0EpsilonOperatorStokesNitscheBC>::value,
                                  "Not implemented for any other operator.");
                    WALBERLA_LOG_INFO_ON_ROOT("Not implemented for non divergence-free solutions!");

                    auto [solFuncX, solFuncY, solFuncZ, pFunc] = solTuple_;
                    auto [rhsFuncX, rhsFuncY, rhsFuncZ, g_expr] = rhsTuple_;

                    // weakly/Nitsche type apply of bcs of corresponding operator velocity block
                    if constexpr (std::is_same<StokesOperatorType, EGP0StokesOperatorNitscheBC>::value) {
                        if (!storage_->hasGlobalCells()) {
                            // corresponding operator forms for application of bcs to rhs
                            auto laplaceForm0 = std::make_shared<dg::eg::EGVectorLaplaceFormNitscheBC_P1P1_00>();
                            auto laplaceForm1 = std::make_shared<dg::eg::EGVectorLaplaceFormNitscheBC_P1P1_11>();
                            auto laplaceFormDG = std::make_shared<dg::eg::EGVectorLaplaceFormNitscheBC_EE>();

                            // check in solution functions
                            laplaceForm0->callback_Scalar_Variable_Coefficient_2D_g0 = solFuncX;
                            laplaceForm1->callback_Scalar_Variable_Coefficient_2D_g1 = solFuncY;
                            laplaceFormDG->callback_Scalar_Variable_Coefficient_2D_g0 = solFuncX;
                            laplaceFormDG->callback_Scalar_Variable_Coefficient_2D_g1 = solFuncY;

                            // Assemble RHS by integration, apply bcs weakly
                            //rhs.uvw().evaluateLinearFunctional( rhsFuncX, rhsFuncY, level );
                            rhs.uvw().applyDirichletBoundaryConditions(laplaceForm0, laplaceForm1, laplaceFormDG,
                                                                       level);
                        } else {
                            // corresponding operator forms for application of bcs to rhs
                            auto laplaceForm0 = std::make_shared<dg::eg::EGVectorLaplaceFormNitscheBC_P1P1_00>();
                            auto laplaceForm1 = std::make_shared<dg::eg::EGVectorLaplaceFormNitscheBC_P1P1_11>();
                            auto laplaceForm2 = std::make_shared<dg::eg::EGVectorLaplaceFormNitscheBC_P1P1_22>();
                            auto laplaceFormDG = std::make_shared<dg::eg::EGVectorLaplaceFormNitscheBC_EE>();

                            // check in solution functions
                            laplaceForm0->callback_Scalar_Variable_Coefficient_3D_g0 = solFuncX;
                            laplaceForm1->callback_Scalar_Variable_Coefficient_3D_g1 = solFuncY;
                            laplaceForm2->callback_Scalar_Variable_Coefficient_3D_g2 = solFuncZ;
                            laplaceFormDG->callback_Scalar_Variable_Coefficient_3D_g0 = solFuncX;
                            laplaceFormDG->callback_Scalar_Variable_Coefficient_3D_g1 = solFuncY;
                            laplaceFormDG->callback_Scalar_Variable_Coefficient_3D_g2 = solFuncZ;

                            // Assemble RHS by integration, apply bcs weakly
                            //rhs.uvw().evaluateLinearFunctional( rhsFuncX, rhsFuncY, rhsFuncZ, level );
                            rhs.uvw().applyDirichletBoundaryConditions(laplaceForm0, laplaceForm1, laplaceForm2,
                                                                       laplaceFormDG, level);
                        }
                    } else {
                        if (!storage_->hasGlobalCells()) {
                            // corresponding operator forms for application of bcs to rhs
                            auto viscosity = Op_->velocityBlockOp.viscosity_;
                            auto epsForm00 = std::make_shared<dg::eg::EGEpsilonFormNitscheBC_P1P1_00>(viscosity);
                            auto epsForm11 = std::make_shared<dg::eg::EGEpsilonFormNitscheBC_P1P1_11>(viscosity);
                            auto epsFormDG = std::make_shared<dg::eg::EGEpsilonFormNitscheBC_EE>(viscosity);

                            // check in solution functions
                            epsForm00->callback_Scalar_Variable_Coefficient_2D_g0 = solFuncX;
                            epsForm00->callback_Scalar_Variable_Coefficient_2D_g1 = solFuncY;

                            epsForm11->callback_Scalar_Variable_Coefficient_2D_g0 = solFuncX;
                            epsForm11->callback_Scalar_Variable_Coefficient_2D_g1 = solFuncY;

                            epsFormDG->callback_Scalar_Variable_Coefficient_2D_g0 = solFuncX;
                            epsFormDG->callback_Scalar_Variable_Coefficient_2D_g1 = solFuncY;

                            // Assemble RHS by integration, apply bcs weakly
                            //rhs.uvw().evaluateLinearFunctional( rhsFuncX, rhsFuncY, rhsFuncZ, level );
                            rhs.uvw().applyDirichletBoundaryConditions(epsForm00, epsForm11, epsFormDG, level);
                        } else {
                            // corresponding operator forms for application of bcs to rhs
                            auto viscosity = Op_->velocityBlockOp.viscosity_;
                            auto epsForm00 = std::make_shared<dg::eg::EGEpsilonFormNitscheBC_P1P1_00>(viscosity);
                            auto epsForm11 = std::make_shared<dg::eg::EGEpsilonFormNitscheBC_P1P1_11>(viscosity);
                            auto epsForm22 = std::make_shared<dg::eg::EGEpsilonFormNitscheBC_P1P1_22>(viscosity);
                            auto epsFormDG = std::make_shared<dg::eg::EGEpsilonFormNitscheBC_EE>(viscosity);

                            // check in solution functions
                            epsForm00->callback_Scalar_Variable_Coefficient_3D_g0 = solFuncX;
                            epsForm00->callback_Scalar_Variable_Coefficient_3D_g1 = solFuncY;
                            epsForm00->callback_Scalar_Variable_Coefficient_3D_g2 = solFuncZ;

                            epsForm11->callback_Scalar_Variable_Coefficient_3D_g0 = solFuncX;
                            epsForm11->callback_Scalar_Variable_Coefficient_3D_g1 = solFuncY;
                            epsForm11->callback_Scalar_Variable_Coefficient_3D_g2 = solFuncZ;

                            epsForm22->callback_Scalar_Variable_Coefficient_3D_g0 = solFuncX;
                            epsForm22->callback_Scalar_Variable_Coefficient_3D_g1 = solFuncY;
                            epsForm22->callback_Scalar_Variable_Coefficient_3D_g2 = solFuncZ;

                            epsFormDG->callback_Scalar_Variable_Coefficient_3D_g0 = solFuncX;
                            epsFormDG->callback_Scalar_Variable_Coefficient_3D_g1 = solFuncY;
                            epsFormDG->callback_Scalar_Variable_Coefficient_3D_g2 = solFuncZ;

                            // Assemble RHS by integration, apply bcs weakly
                            //   rhs.uvw().evaluateLinearFunctional( rhsFuncX, rhsFuncY, rhsFuncZ, level );
                            rhs.uvw().applyDirichletBoundaryConditions(epsForm00, epsForm11, epsForm22, epsFormDG,
                                                                       level);
                        }
                    }

                    // weakly/Nitsche type apply of div BCs
                    auto divForm = std::make_shared<dg::eg::EGDivFormNitscheBC_P0E>();
                    if (!storage_->hasGlobalCells()) {
                        divForm->callback_Scalar_Variable_Coefficient_2D_g0 = solFuncX;
                        divForm->callback_Scalar_Variable_Coefficient_2D_g1 = solFuncY;
                    } else {
                        divForm->callback_Scalar_Variable_Coefficient_3D_g0 = solFuncX;
                        divForm->callback_Scalar_Variable_Coefficient_3D_g1 = solFuncY;
                        divForm->callback_Scalar_Variable_Coefficient_3D_g2 = solFuncZ;
                    }
                    rhs.p().interpolate(0, level, All);
                    rhs.p().getDGFunction()->applyDirichletBoundaryConditions(divForm, level);
                }

                ErrorArray RunStokesTestOnLevel(const uint_t &level) {
                    StokesFunctionNumeratorType numerator("numerator", storage_, level, level);
                    numerator.enumerate(level);
                    uint_t globalDoFs = numberOfGlobalDoFs(numerator, level);
                    WALBERLA_LOG_INFO_ON_ROOT("Global DoFs: " << globalDoFs);

                    // solution, rhs as a lambda function
                    auto [u_x_expr, u_y_expr, u_z_expr, p_expr] = solTuple_;
                    auto [f_x_expr, f_y_expr, f_z_expr, g_expr] = rhsTuple_;

                    StokesFunctionType u("u", storage_, level, level + 1);
                    StokesFunctionType f("f", storage_, level, level);
                    StokesFunctionType rhs("rhs", storage_, level, level);
                    StokesFunctionType sol("sol", storage_, level, level + 1);
                    StokesFunctionType err("err", storage_, level, level + 1);
                    StokesFunctionType Merr("Merr", storage_, level, level + 1);

                    if constexpr (isEGP0Discr<StokesOperatorType>()) {
                        copyBdry(u);
                        copyBdry(f);
                        copyBdry(rhs);
                        copyBdry(sol);
                        copyBdry(err);
                        copyBdry(Merr);
                    }

                    // interpolate analytical solution and rhs
                    if (!storage_->hasGlobalCells()) {
                        sol.uvw().interpolate({u_x_expr, u_y_expr}, level, All);
                        sol.uvw().interpolate({u_x_expr, u_y_expr}, level + 1, All);
                        f.uvw().interpolate({f_x_expr, f_y_expr}, level, All);
                    } else {
                        sol.uvw().interpolate({u_x_expr, u_y_expr, u_z_expr}, level, All);
                        sol.uvw().interpolate({u_x_expr, u_y_expr, u_z_expr}, level + 1, All);
                        f.uvw().interpolate({f_x_expr, f_y_expr, f_z_expr}, level, All);
                    }
                    sol.p().interpolate(p_expr, level, All);
                    sol.p().interpolate(p_expr, level + 1, All);
                    f.p().interpolate(g_expr, level, All);

                    // setup rhs linear form: with mass matrix inexactly or by numerical integration
                    if constexpr (usesNitscheBCs<StokesOperatorType>()) {
                        setupRHSinexact(level, f, rhs);
                        integrateRHS(level, rhs);
                        setupBC(level, u);
                    } else {
                        setupRHSinexact(level, f, rhs);
                        setupBC(level, u);
                    }

                    // solve
                    switch (solverType_) {
                        case 0: {
                            MinResSolver<StokesOperatorType> solver(storage_, level, level,
                                                                    std::numeric_limits<uint_t>::max(), residualTol_);
                            solver.setPrintInfo(true);
                            solver.solve(*Op_, u, rhs, level);
                            break;
                        }
                        case 1: {
                            PETScLUSolver<StokesOperatorType> solver(storage_, level);
                            solver.disableApplicationBC(usesNitscheBCs<StokesOperatorType>());
                            /*StokesFunctionType nullSpace("ns", storage_, level, level);
                                               nullSpace.uvw().interpolate(0, level, All);
                                               nullSpace.p().interpolate(1, level, All);
                                               solver.setNullSpace(nullSpace, level);*/
                            solver.solve(*Op_, u, rhs, level);
                            break;
                        }

                        case 2: {
                            PETScBlockPreconditionedStokesSolver<StokesOperatorType> solver(storage_,
                                                                                            level,
                                                                                            residualTol_,
                                                                                            std::numeric_limits<PetscInt>::max(),
                                                                                            6,
                                                                                            1);
                            solver.disableApplicationBC(usesNitscheBCs<StokesOperatorType>());
                            solver.solve(*Op_, u, rhs, level);

                            break;
                        }

                        default: {
                            PETScMinResSolver<StokesOperatorType> solver(storage_, level, numerator, residualTol_,
                                                                         residualTol_);
                            solver.setFromOptions(true);
                            solver.disableApplicationBC(usesNitscheBCs<StokesOperatorType>());
                            solver.solve(*Op_, u, rhs, level);
                            break;
                        }
                    }

                    // pressure projection to space of mean-value-0-functions
                    if constexpr (isEGP0Discr<StokesOperatorType>() || isP1P0Discr<StokesOperatorType>()) {
                        hyteg::dg::projectMean(u.p(), level);
                        hyteg::dg::projectMean(sol.p(), level);
                    } else if constexpr (isP2P1Discr<StokesOperatorType>()) {
                        hyteg::vertexdof::projectMean(u.p(), level);
                        hyteg::vertexdof::projectMean(sol.p(), level);
                    }

                    // post evaluation of the numerical solution
                    if (maxLevel_ == level && checkSolution_ != nullptr)
                        (*checkSolution_)(u, MeshQuality::getMaximalEdgeLength(storage_, level));

                    // compute error
                    err.assign({1.0, -1.0}, {u, sol}, level, All);

                    if constexpr (isP2P1Discr<StokesOperatorType>()) {
                        // map to finer grid to counter superconvergence
                        P2toP2QuadraticProlongation P2P2ProlongationOp;
                        for (uint_t k = 0; k < u.uvw().getDimension(); k++) {
                            P2P2ProlongationOp.prolongate(u.uvw()[k], level, Inner);
                        }
                        err.assign({1.0, -1.0}, {u, sol}, level + 1, Inner);
                    } else if constexpr (isEGP0Discr<StokesOperatorType>()) {
                        P1toP1LinearProlongation P1P1ProlongationOp;
                        for (uint_t k = 0; k < u.uvw().getConformingPart()->getDimension(); k++) {
                            P1P1ProlongationOp.prolongate((*u.uvw().getConformingPart())[k], level, Inner);
                        }
                        err.assign({1.0, -1.0}, {u, sol}, level + 1, All);
                    }

                    if (writeVTK_) {
                        VTKOutput vtk("/mnt/c/Users/Fabia/OneDrive/Desktop/hyteg_premerge_2/hyteg/output", testName_,
                                      storage_);
                        if constexpr (isEGP0Discr<StokesOperatorType>()) {
                            vtk.add(u);
                            vtk.add(*u.uvw().getConformingPart());
                            vtk.add(*u.uvw().getDiscontinuousPart());
                            vtk.add(f);
                            vtk.add(*f.uvw().getConformingPart());
                            vtk.add(*f.uvw().getDiscontinuousPart());
                            vtk.add(rhs);
                            vtk.add(*rhs.uvw().getConformingPart());
                            vtk.add(*rhs.uvw().getDiscontinuousPart());
                            vtk.add(sol);
                            vtk.add(*sol.uvw().getConformingPart());
                            vtk.add(*sol.uvw().getDiscontinuousPart());
                            vtk.add(err);
                            vtk.add(*err.uvw().getConformingPart());
                            vtk.add(*err.uvw().getDiscontinuousPart());
                            vtk.add(f);
                        } else {
                            // P2Function<real_t> viscosity("viscosity", storage_, level, level);
                            //viscosity.interpolate(Op_->viscOp.viscosity_, level, All);
                            //vtk.add(viscosity);
                            vtk.add(u);
                            vtk.add(sol);
                            vtk.add(err);
                            vtk.add(f);
                            vtk.add(rhs);
                        }

                        vtk.write(level, 1);
                    }

                    // check for energyNormOp
                    static_assert(hasEnergyNormOp<StokesOperatorType>,
                                  "Member energyOp must be implemented in the used Stokes operator type.");

                    // pack returns: DoFs, iteration number, error norms
                    std::vector<real_t> ret = {real_c(globalDoFs)};
                    auto norms = EGNormComputer(level, err, storage_).compute(Op_->energyNormOp);
                    ret.insert(ret.end(), norms.begin(), norms.end());

                    // check if the desired threshhold on the velocity error is reached
                    if (discrErrorThreshhold_.first) {
                        WALBERLA_CHECK_LESS(norms[0], discrErrorThreshhold_.second,
                                            "Threshhold for discretization error not reached.");
                    }

                    return ret;
                }

                std::string testName_;
                uint_t maxLevel_;
                LambdaTuple solTuple_;
                LambdaTuple rhsTuple_;
                std::shared_ptr<StokesOperatorType> Op_;
                std::shared_ptr<PrimitiveStorage> storage_;
                uint_t solverType_;
                bool writeVTK_;
                real_t residualTol_;
                std::shared_ptr<checkFunctionType> checkSolution_;
                std::pair<bool, real_t> discrErrorThreshhold_;
            };

        } // namespace eg
    } // namespace dg
} // namespace hyteg
