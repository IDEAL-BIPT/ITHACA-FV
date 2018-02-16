/*---------------------------------------------------------------------------*\
     ██╗████████╗██╗  ██╗ █████╗  ██████╗ █████╗       ███████╗██╗   ██╗
     ██║╚══██╔══╝██║  ██║██╔══██╗██╔════╝██╔══██╗      ██╔════╝██║   ██║
     ██║   ██║   ███████║███████║██║     ███████║█████╗█████╗  ██║   ██║
     ██║   ██║   ██╔══██║██╔══██║██║     ██╔══██║╚════╝██╔══╝  ╚██╗ ██╔╝
     ██║   ██║   ██║  ██║██║  ██║╚██████╗██║  ██║      ██║      ╚████╔╝ 
     ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝╚═╝  ╚═╝      ╚═╝       ╚═══╝  
 
 * In real Time Highly Advanced Computational Applications for Finite Volumes 
 * Copyright (C) 2017 by the ITHACA-FV authors
-------------------------------------------------------------------------------

License
    This file is part of ITHACA-FV

    ITHACA-FV is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    ITHACA-FV is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with ITHACA-FV. If not, see <http://www.gnu.org/licenses/>.

Description
    Example of NS-Stokes Reduction Problem

\*---------------------------------------------------------------------------*/


#include "steadyNS.H"
#include "ITHACAstream.H"
#include "ITHACAPOD.H"
#include "reducedSteadyNS.H"

/// \brief Class where the tutorial number 3 is implemented.
/// \details It is a child of the steadyNS class and some of its 
/// functions are overridden to be adapted to the specific case.
class tutorial03 : public steadyNS
{
public:
	/// Constructor
	explicit tutorial03(int argc, char *argv[])
		:
		steadyNS(argc, argv),
		U(_U()),
		p(_p())
	{}

	/// Velocity field
	volVectorField& U;
	/// Pressure field
	volScalarField& p;

	/// Perform an Offline solve
	void offlineSolve()
	{
		Vector<double> inl(0, 0, 0);
		List<scalar> mu_now(1);
		// if the offline solution is already performed read the fields
		if (offline)
		{
			ITHACAstream::read_fields(Ufield, U, "./ITHACAoutput/Offline/");
			ITHACAstream::read_fields(Pfield, p, "./ITHACAoutput/Offline/");
		}
		else
		{
			Vector<double> Uinl(0, 0, 0);
			label BCind = 0;
			for (label i = 0; i < mu.rows(); i++)
			{
				change_viscosity( mu(i, 0));
				assignIF(U, Uinl);
				truthSolve();
			}

		}
	}

};

int main(int argc, char *argv[])
{
	// Construct the tutorial object
	tutorial03 example(argc, argv);

	// Read the par file where the parameters are stored
	word filename("./par");
	example.mu = ITHACAstream::readMatrix(filename);


	// Set the inlet boundaries patch 0 directions x and y
	example.inletIndex.resize(1, 2);
	example.inletIndex(0, 0) = 0;
	example.inletIndex(0, 1) = 0;

	// Perform the offline solve
	example.offlineSolve();

	// Solve the supremizer problem
	example.solvesupremizer();
	
	ITHACAstream::read_fields(example.liftfield, example.U, "./lift/");
	
	// Homogenize the snapshots
	example.computeLift(example.Ufield, example.liftfield, example.Uomfield);



	// Perform POD on velocity pressure and supremizers and store the first 50 modes
	ITHACAPOD::getModes(example.Uomfield, example.Umodes, example.podex, 0, 0, 10);
	ITHACAPOD::getModes(example.Pfield, example.Pmodes, example.podex, 0, 0, 10);
	ITHACAPOD::getModes(example.supfield, example.supmodes, example.podex, example.supex, 1, 10);

	// Perform the Galerkin Projection
	example.projectSUP("./Matrices", 5, 5 , 5);

	// Create the reduced object
	reducedSteadyNS ridotto(example, "SUP");

	Eigen::MatrixXd vel_now(2, 1);
	vel_now(0, 0) = 1;
	vel_now(1, 0) = 0;

	// Perform an online solve for the new values of inlet velocities
	for (label k = 0; k < 20; k++)
	{

		// Set the reduced viscosity
		ridotto.nu = example.mu(k, 0);
		ridotto.solveOnline_sup(vel_now);
		Eigen::MatrixXd tmp_sol(ridotto.y.rows() + 1, 1);
		tmp_sol(0) = k + 1;
		tmp_sol.col(0).tail(ridotto.y.rows()) = ridotto.y;
		ridotto.online_solution.append(tmp_sol);
	}
	// Save the online solution
	ITHACAstream::exportMatrix(ridotto.online_solution, "red_coeff", "python", "./ITHACAoutput/red_coeff");
	ITHACAstream::exportMatrix(ridotto.online_solution, "red_coeff", "matlab", "./ITHACAoutput/red_coeff");
	ITHACAstream::exportMatrix(ridotto.online_solution, "red_coeff", "eigen", "./ITHACAoutput/red_coeff");

	// Reconstruct and export the solution
	ridotto.reconstruct_sup(example, "./ITHACAoutput/Reconstruction/");
	exit(0);
}

//--------
/// \dir 03steadyNS Folder of the turorial 3
/// \file 
/// \brief Implementation of a tutorial of a steady Navier-Stokes problem

/// \example 03steadyNS.C
/// \section intro_sreadyNS Introduction to tutorial 3
/// The problems consists of steady Navier-Stokes problem with parametrized viscosity.
/// The physical problem is the backward facing step depicted in the following image:
/// \image html step.png
/// At the inlet a uniform and constant velocity equal to 1 m/s is prescribed. 
/// 
/// \section code A look under the code
/// 
/// In this section are explained the main steps necessary to construct the tutorial N°3
/// 
/// \subsection header The necessary header files
/// 
/// First of all let's have a look to the header files that needs to be included and what they are responsible for:
/// 
/// The header file of ITHACA-FV necessary for this tutorial
/// 
/// \dontinclude 03steadyNS.C
/// \skip steadyNS
/// \until reducedSteady
/// 
/// \subsection classtuto03 Implementation of the tutorial03 class 
/// 
/// Then we can define the tutorial03 class as a child of the steadyNS class
/// \skipline tutorial03
/// \until {}
/// 
/// The members of the class are the fields that needs to be manipulated during the
/// resolution of the problem
/// 
/// Inside the class it is defined the offlineSolve method according to the
/// specific parametrized problem that needs to be solved.
/// 
/// \skipline offlineSolve
/// \until {
/// 
/// 
/// If the offline solve has already been performed than read the existing snapshots
/// 
/// \skipline offline
/// \until }
///
/// else perform the offline solve where a loop over all the parameters is performed:
/// 
/// \skipline else
/// \until }
/// \skipline }
/// 
/// See also the steadyNS class for the definition of the methods.
/// 
/// \subsection main Definition of the main function
/// 
/// Once the tutorial03 class is defined the main function is defined,
/// an example of type tutorial03 is constructed:
/// 
/// \skipline tutorial03		
/// 
/// In this case the vector of parameter is read from a txt file
/// 
/// \skipline word
/// \until example.mu
/// 
/// The inlet boundary is set:
/// 
/// \skipline example.inlet
/// \until example.inletIndex(0, 1) = 0;
/// 
/// and the offline stage is performed:
/// 
/// \skipline Solve()
/// 
/// and the supremizer problem is solved:
/// 
/// \skipline supremizer()
/// 
/// In order to show the functionality of reading fields in this case the lifting function is read
/// from a precomputed simulation with a unitary inlet velocity:
/// 
/// \skipline stream
/// 
/// Then the snapshots matrix is homogenized:
/// 
/// \skipline computeLift
/// 
/// and the modes for velocity, pressure and supremizers are obtained:
/// 
/// \skipline getModes
/// \until supfield
/// 
/// then the projection onto the POD modes is performed with:
/// 
/// \skipline projectSUP
/// 
/// the reduced object is constructed:
/// 
/// \skipline reducedSteady
/// 
/// and the online solve is performed for some values of the viscosity:
/// 
/// \skipline Eigen::
/// \until }
/// 
/// The vel_now matrix in this case is not used since there are no parametrized boundary conditions.
/// 
/// The viscosity is set with the command:
/// 
/// \code
/// ridotto.nu = example.mu(k,0)
/// \endcode
/// 
/// finally the online solution stored during the online solve is exported to file in three different
/// formats with the lines:
/// 
/// \skipline exportMatrix
/// \until "eigen"
/// 
/// and the online solution is reconstructed and exported to file
/// 
/// \skipline reconstruct
/// 
/// 
/// 
/// 
/// 
/// \section plaincode The plain program
/// Here there's the plain code
/// 


