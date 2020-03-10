// Author: Richard Glennie
// Date: Feb 2020
// Continuous-time Markov chain with GAMs 

#include <TMB.hpp>
#include <iostream>

template<class Type>
Type objective_function<Type>::operator() ()
{
  
  using namespace R_inla;
  using namespace density;
  using namespace Eigen;
  
  DATA_INTEGER(flag); // flag=0 => only prior
  DATA_MATRIX(Xdive); // dive design matrix
  DATA_MATRIX(Xsurf); // surface design matrix
  DATA_SPARSE_MATRIX(S_dive); // smoothing matrix
  DATA_SPARSE_MATRIX(S_surface); // smoothing matrix
  DATA_MATRIX(A_dive); // projector for dive times
  DATA_MATRIX(A_surf); // projector for surface times
  DATA_MATRIX(Xs_grid_surface); // projector for integration (fixed effects)
  DATA_MATRIX(Xs_grid_dive); // projector for integration (fixed effects)
  DATA_MATRIX(A_grid_surface); // projector for integration (random effects)
  DATA_MATRIX(A_grid_dive); // projector for integration (random effects)
  DATA_VECTOR(W_dive); 
  DATA_VECTOR(W_surf); 
  DATA_VECTOR(W_grid_dive); 
  DATA_VECTOR(W_grid_surf); 
  DATA_SPARSE_MATRIX(indD); // integration points within surfacings
  DATA_SPARSE_MATRIX(indS); // integration point within dives
  DATA_SCALAR(dt); // time step in integration
  DATA_IVECTOR(S_dive_n); // sizes of sparse matrices
  DATA_IVECTOR(S_surface_n); // sizes of sparse matrices
  
  PARAMETER_VECTOR(par_dive); // dive parameters
  PARAMETER_VECTOR(par_surf); // surface parameters
  PARAMETER(log_kappa_dive); 
  PARAMETER(log_kappa_surf); 
  PARAMETER_VECTOR(log_lambda_dive); // dive log smoothing parameter
  PARAMETER_VECTOR(log_lambda_surf); // surface log smoothing parameter
  PARAMETER_VECTOR(s_dive); // dive random effects
  PARAMETER_VECTOR(s_surf); // surface random effects
  
  vector<Type> lambda_dive = exp(log_lambda_dive); // dive smoothing parameter
  vector<Type> lambda_surf = exp(log_lambda_surf); // surface smoothing parameter
  Type kappa_dive = exp(log_kappa_dive); 
  Type kappa_surf = exp(log_kappa_surf); 
  
  // Negative log-likelihood
  Type nll = 0;
  
  // add smoothing penalties, need to do this wonky bit to unblock S in each case
  // data setup
  int S_start = 0;
  if (S_dive_n(0) > 0.5) {
    // dive bit
    for(int i = 0; i < S_dive_n.size(); i++) {
      int Sn = S_dive_n(i);
      SparseMatrix<Type> this_S = S_dive.block(S_start, S_start, Sn, Sn);
      vector<Type> beta_s = s_dive.segment(S_start, Sn);
      nll -= Type(0.5) * Sn * log_lambda_dive(i) - 0.5 * lambda_dive(i) * GMRF(this_S, false).Quadform(beta_s);
      S_start += Sn;
    }
  }
  
  if (S_surface_n(0) > 0.5) {
    // surface bit
    S_start = 0;
    for(int i = 0; i < S_surface_n.size(); i++) {
      int Sn = S_surface_n(i);
      SparseMatrix<Type> this_S = S_surface.block(S_start, S_start, Sn, Sn);
      vector<Type> beta_s = s_surf.segment(S_start, Sn);
      nll -= Type(0.5) * Sn * log_lambda_surf(i) - 0.5 * lambda_surf(i) * GMRF(this_S, false).Quadform(beta_s);
      S_start += Sn;
    }
  }
  
  // Return un-normalized density on request
  // used when running TMB::normalize to obtain GMRF normalization
  if (flag == 0) return nll;
  
  // calculate log intensities
  vector<Type> le_dive = Xdive * par_dive; 
  if (S_dive_n(0) > 0.5) le_dive += A_dive * s_dive;
  le_dive = kappa_dive * le_dive + (kappa_dive - 1) * W_dive; 
  le_dive = (le_dive.array() + log_kappa_dive).matrix(); 
  vector<Type> le_surf  = Xsurf * par_surf; 
  if (S_surface_n(0) > 0.5) le_surf += A_surf * s_surf;
  le_surf = kappa_surf * le_surf + (kappa_surf - 1) * W_surf; 
  le_surf = (le_surf.array() + log_kappa_surf).matrix(); 
  
  // Integral of dive intensity
  vector<Type> int_dive = Xs_grid_dive * par_dive; 
  if (S_dive_n(0) > 0.5) int_dive += A_grid_dive * s_dive;
  int_dive = kappa_dive * int_dive + (kappa_dive - 1) * W_grid_dive; 
  int_dive = (int_dive.array() + log_kappa_dive).matrix(); 
  int_dive = exp(int_dive);
  int_dive = indD * int_dive;
  // integral is t_i-u_{i-1}, t_n
  vector<Type> subint_dive = int_dive.head(int_dive.size() - 1);
  subint_dive *= dt;
  
  // Integral of surface intensity
  vector<Type> int_surf = Xs_grid_surface * par_surf; 
  if (S_surface_n(0) > 0.5) int_surf += A_grid_surface * s_surf;
  int_surf = kappa_surf * int_surf + (kappa_surf - 1) * W_grid_surf; 
  int_surf = (int_surf.array() + log_kappa_surf).matrix(); 
  int_surf = exp(int_surf);
  int_surf = indS * int_surf;
  int_surf *= dt;
  
  // Likelihood contributions
  for(int i = 0; i < le_dive.size(); i++) {
    nll -= le_surf(i) - int_surf(i);
    if (i > 0) nll -= le_dive(i); 
    if (i < le_dive.size() - 1) nll -= -subint_dive(i);
  }
  
  return nll;
}
