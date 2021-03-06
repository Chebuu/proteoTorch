/*    Copyright 2006 Vikas Sindhwani (vikass@cs.uchicago.edu)
      SVM-lin: Fast SVM Solvers for Supervised and Semi-supervised Learning
 ******************************
 * Sped up by John Halloran at the University of California, Davis, as detailed in:
 ******************************
 * A Matter of Time: Faster Percolator Analysis via Efficient SVM Learning for 
 * Large-Scale Proteomics
 * John T. Halloran and David M. Rocke
 * Journal of Proteome Research 2018 17 (5), 1978-1982
 ******************************
      This file is part of SVM-lin.

      SVM-lin is free software; you can redistribute it and/or modify
      it under the terms of the GNU General Public License as published by
      the Free Software Foundation; either version 2 of the License, or
      (at your option) any later version.

      SVM-lin is distributed in the hope that it will be useful,
      but WITHOUT ANY WARRANTY; without even the implied warranty of
      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
      GNU General Public License for more details.

      You should have received a copy of the GNU General Public License
      along with SVM-lin (see gpl.txt); if not, write to the Free Software
      Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <cmath>
#include <ctype.h>
#include "ssl.h"

#include <stdarg.h>
#include <cstring>

#ifdef __cplusplus
extern "C" {
#endif
  extern double dnrm2_(int *, double *, int *); // Return the Euclidian norm of a vector
  extern double ddot_(int *, double *, int *, double *, int *); // compute the dot product of two vectors
  extern int daxpy_(int *, double *, double *, int *, double *, int *); // compute y := alpha * x + y
  extern int dscal_(int *, double *, double *, int *); // Compute y := alpha * y
  // dgemv - perform one of the matrix-vector operations, y  :=
  // alpha*A*x + beta*y or y := alpha*A'*x + beta*y
  extern int dgemv_(char *, int *, int *,
                    double *, double *, int *,
                    double *, int *,  double *,
                    double *, int *);
#ifdef __cplusplus
}
#endif

#define VERBOSE 1
#define LOG2(x) 1.4426950408889634*log(x) 
// for compatibility issues, not using log2

using namespace std;

double cglsFun1(int active, int* J, const double* Y,
                double* set2, int n, double* q, 
                double* p, double cpos, double cneg){
  double omega_q = 0.0;
  int inc = 1;
  int i = 0;

  char trans = 'T';
  double alpha = 1.0;
  double beta = 0.0;
  dgemv_(&trans, &n, &active,
         &alpha, set2, &n,
         p, &inc, &beta, q, &inc);

  for (i = 0; i < active; i++) {
    omega_q += ((Y[J[i]]==1)? cpos : cneg) * (q[i]) * (q[i]);
  }

  return(omega_q);
}

void cglsFun2(int active, int* J, const double* Y,
              double* set2, int n0, int n, double* q, 
              double* o, double* z, double* r, 
              double cpos, double cneg){
  int i;
  int inc = 1;
  int ind = 0;
  double temp = 0.0;
  
  for (i = 0; i < active; i++) {
    o[J[i]] += q[i];
    z[i] -= ((Y[J[i]]==1)? cpos : cneg) * q[i];
    daxpy_(&n, &(z[i]), set2 + i * n, &inc, r, &inc);
  }
}

int CGLS(const struct data *Data, 
	 const int cgitermax,
         const double epsilon,
	 const struct vector_int *Subset, 
	 struct vector_double *Weights,
	 struct vector_double *Outputs,
	 int verbose, double cpos, double cneg)
{
  if(VERBOSE_CGLS)
    cout << "CGLS starting..." << endl;
  /* Disassemble the structures */
  timer tictoc;
  tictoc.restart();
  int active = Subset->d;
  int *J = Subset->vec;
  double* set = Data->X;
  double *Y = Data->Y;
  // double *C = Data->C;
  int n  = Data->n;
  int n1 = n-1;
  double lambda_l = 1.0;
  double *beta = Weights->vec;
  double *o  = Outputs->vec; 
  // initialize z 
  double *z = new double[active];
  double *q = new double[active];
  int ii=0;
  register int i,j; 
  int n0 = n-1;
  int inc = 1;
  double one = 1;
  double negLambda = -lambda_l;
  int rowStart = 0;
  double* set2 = new double[n*active];
  double* r = new double[n];
  for (i = n; i--;) {
    r[i] = 0.0;
  }
  for (i = 0; i < active; i++) {
    ii = J[i];
    z[i] = ((Y[ii]==1)? cpos : cneg) * (Y[ii] - o[ii]);
    rowStart = i * n;
    memcpy(set2 + rowStart, set + ii*n1, sizeof(double)*n1);
    set2[rowStart + n1] = 1;
    daxpy_(&n, &(z[i]), set2 + i*n, &inc, r, &inc);
  }
  double *p = new double[n];   
  // double omega1 = 0.0;
  // for(i = n ; i-- ;)
  //   {
  //     r[i] -= lambda_l*beta[i];
  //     p[i] = r[i];
  //     omega1 += r[i]*r[i];
  //   }   
  // double omega_p = omega1;
  // double omega_q = 0.0;
  // double inv_omega2 = 1/omega1;
  // double scale = 0.0;
  // double omega_z=0.0;
  // double gamma = 0.0;
  // int cgiter = 0;
  // int optimality = 0;
  // double epsilon2 = epsilon*epsilon;   
  daxpy_(&n, &negLambda, beta, &inc, r, &inc);
  memcpy(p, r, sizeof(double)*n);
  double omega1 = ddot_(&n, r, &inc, r, &inc);
  double omega_p = omega1;
  double omega_q = 0.0;
  double inv_omega2 = 1 / omega1;
  double scale = 0.0;
  double omega_z = 0.0;
  double gamma = 0.0;
  int cgiter = 0;
  int optimality = 0;
  double epsilon2 = epsilon * epsilon;
  // iterate
  while(cgiter < cgitermax)
    {
      cgiter++;
      omega_q = cglsFun1(active, J, Y, set2, n, q, p, cpos, cneg);
      gamma = omega1 / (lambda_l * omega_p + omega_q);
      inv_omega2 = 1 / omega1;

      memcpy(r, beta, sizeof(double)*n);
      dscal_(&n, &negLambda, r, &inc);

      daxpy_(&n, &gamma, p, &inc, beta, &inc);
      dscal_(&active, &gamma, q, &inc);

      cglsFun2(active, J, Y, set2,
	       n0, n, q, o, z, r, cpos, cneg);

      omega_z = ddot_(&active, z, &inc, z, &inc);
      omega1 = ddot_(&n, r, &inc, r, &inc);

      if (VERBOSE_CGLS) {
	cout << "..." << cgiter << " ( " << omega1 << " )";
      }

      if (omega1 < epsilon2 * omega_z) {
	optimality = 1;
	break;
      }

      scale = omega1 * inv_omega2;
      dscal_(&n, &scale, p, &inc);
      daxpy_(&n, &one, r, &inc, p, &inc);
      omega_p = ddot_(&n, p, &inc, p, &inc);
      // omega_q=0.0;
      // double t=0.0;
      // for(i=0; i < active; i++)
      // 	{
      // 	  ii=J[i];
      // 	  t=p[n - 1];
      // 	  for (j = 0; j < n - 1; j++) {
      // 	    t += set[j + ii * n] * p[j];
      // 	  }
      // 	  q[i]=t;
      // 	  omega_q += C[ii]*t*t;
      // 	}       
      // gamma = omega1/(lambda_l*omega_p + omega_q);    
      // inv_omega2 = 1/omega1;     
      // for(int i = n ; i-- ;)
      // 	{
      // 	  r[i] = 0.0;
      // 	  beta[i] += gamma*p[i];
      // 	} 
      // omega_z=0.0;
      // for(int i = active ; i-- ;)
      // 	{
      // 	  ii=J[i];
      // 	  o[ii] += gamma*q[i];
      // 	  z[i] -= gamma*C[ii]*q[i];
      // 	  omega_z+=z[i]*z[i];
      // 	} 
      // for(register int j=0; j < active; j++)
      // 	{
      // 	  ii=J[j];
      // 	  t=z[j];
      // 	  for (register int i = 0; i < n - 1; i++) {
      // 	    r[i] += set[i + ii * n] * t;
      // 	  }
      // 	  r[n - 1] += t;
      // 	}
      // omega1 = 0.0;
      // for(int i = n ; i-- ;)
      // 	{
      // 	  r[i] -= lambda_l*beta[i];
      // 	  omega1 += r[i]*r[i];
      // 	}
      // if(VERBOSE_CGLS)
      // 	cout << "..." << cgiter << " ( " << omega1 << " )" ; 
      // if(omega1 < epsilon2*omega_z)
      // 	{
      // 	  optimality=1;
      // 	  break;
      // 	}
      // omega_p=0.0;
      // scale=omega1*inv_omega2;
      // for(int i = n ; i-- ;)
      // 	{
      // 	  p[i] = r[i] + p[i]*scale;
      // 	  omega_p += p[i]*p[i]; 
      // 	} 
    }            
  if(VERBOSE_CGLS)
    cout << "...Done." << endl;
  tictoc.stop();
  if (verbose > 0)
    cout << "CGLS converged in " << cgiter << " iteration(s) and " << tictoc.time() << " seconds." << endl;
  delete[] z;
  delete[] q;
  delete[] r;
  delete[] p;
  return optimality;
}
int L2_SVM_MFN(const struct data *Data, 
	       struct options *Options, 
	       struct vector_double *Weights,
	       struct vector_double *Outputs,
	       int verbose, double cpos, double cneg)
{ 
  /* Disassemble the structures */  
  timer tictoc;
  tictoc.restart();
  double* set = Data->X;
  double *Y = Data->Y;
  // double *C = Data->C;
  int n  = Data->n;
  int m  = Data->m;
  int n0 = n-1;
  double lambda_l = 1.0;
  double epsilon = BIG_EPSILON;
  int cgitermax = SMALL_CGITERMAX;
  Options->epsilon=EPSILON;
  double *w = Weights->vec;
  double *o = Outputs->vec; 
  double F_old = 0.0;
  double F = 0.0;
  double diff=0.0;
  int ini = 0;
  int inc = 1;
  vector_int *ActiveSubset = new vector_int[1];
  ActiveSubset->vec = new int[m];
  ActiveSubset->d = m;
  for(int i=0;i<n;i++) F+=w[i]*w[i];
  F=0.5*lambda_l*F;        
  int active=0;
  int inactive=m-1; // l-1      
  for(int i=0; i<m ; i++)
    { 
      diff=1-Y[i]*o[i];
      if(diff>0)
	{
	  ActiveSubset->vec[active]=i;
	  active++;
	  // C[i]
	  F += 0.5 * ((Y[i]==1)? cpos : cneg) * diff * diff;
	  // F+=0.5*C[i]*diff*diff;
	}
      else
	{
	  ActiveSubset->vec[inactive]=i;
	  inactive--;
	}   
    }
  ActiveSubset->d=active;        
  int iter=0;
  int opt=0;
  int opt2=0;
  vector_double *Weights_bar = new vector_double[1];
  vector_double *Outputs_bar = new vector_double[1];
  double *w_bar = new double[n];
  double *o_bar = new double[m];
  Weights_bar->vec=w_bar;
  Outputs_bar->vec=o_bar;
  Weights_bar->d=n;
  Outputs_bar->d=m;
  double delta=0.0;
  double t=0.0;
  int ii = 0;
  while(iter<MFNITERMAX)
    {
      iter++;
      if (verbose > 0)
        cout << "L2_SVM_MFN Iteration# " << iter << " (" << active << " active examples, " << " objective_value = " << F << ")" << endl;
      for(int i=n; i-- ;) 
	w_bar[i]=w[i];
      for(int i=m; i-- ;)  
	o_bar[i]=o[i];
      cout << " " ;
      opt=CGLS(Data,cgitermax, epsilon,ActiveSubset,Weights_bar,Outputs_bar, verbose, cpos, cneg);
      for(register int i=active; i < m; i++) 
	{
	  ii=ActiveSubset->vec[i];   
	  // o_bar[ii] = ddot_(&n, set + ii*n, &inc, w_bar, &inc);
	  o_bar[ii] = ddot_(&n0, set + ii*n0, &inc, w_bar, &inc) + w_bar[n0];
	  // t = w_bar[n - 1];
	  // for (register int j = n - 1; j--;) {
	  //   t += set[j + ii * n] * w_bar[j];
	  // }
	  // o_bar[ii]=t;
	}
      if(ini==0) {
	cgitermax=CGITERMAX; 
	ini=1;
      }
      opt2=1;
      for(int i=0;i<m;i++)
	{ 
	  ii=ActiveSubset->vec[i];
	  if(i<active)
	    opt2=(opt2 && (Y[ii]*o_bar[ii]<=1+epsilon));
	  else
	    opt2=(opt2 && (Y[ii]*o_bar[ii]>=1-epsilon));  
	  if(opt2==0) break;
	}      
      if(opt && opt2) // l
	{
	  if(epsilon==BIG_EPSILON) 
	    {
	      epsilon=EPSILON;
	      Options->epsilon=EPSILON;
	      if (verbose > 0)
		cout << "  epsilon = " << BIG_EPSILON << " case converged (speedup heuristic 2). Continuing with epsilon=" <<  EPSILON << endl;
	      continue;
	    }
	  else
	    {
	      memcpy(w, w_bar, sizeof(double)*n);
	      memcpy(o, o_bar, sizeof(double)*m);
	      delete[] ActiveSubset->vec;
	      delete[] ActiveSubset;
	      delete[] o_bar;
	      delete[] w_bar;
	      delete[] Weights_bar;
	      delete[] Outputs_bar;
	      tictoc.stop();
	      if (verbose > 0)
		cout << "L2_SVM_MFN converged (optimality) in " << iter << " iteration(s) and "<< tictoc.time() << " seconds. \n" << endl;
	      return 1;      
	    }
	}
      if (verbose > 0)
        cout << " " ;
      delta=line_search(w,w_bar,lambda_l,o,o_bar,Y,n,m, cpos, cneg); 
      if (verbose > 0)
        cout << "LINE_SEARCH delta = " << delta << endl;     
      F_old = F;
      double delta2 = 1-delta;
      dscal_(&n, &delta2, w, &inc);
      daxpy_(&n, &delta, w_bar, &inc, w, &inc);
      F = 0.5 * lambda_l * ddot_(&n, w, &inc, w, &inc);
      active = 0;
      inactive = m - 1;
      for (int i = 0; i < m; i++) {
	o[i] += delta * (o_bar[i] - o[i]);
	diff = 1 - Y[i] * o[i];
	if (diff > 0) {
	  ActiveSubset->vec[active] = i;
	  active++;
	  F += 0.5 * ((Y[i]==1)? cpos : cneg) * diff * diff;
	} else {
	  ActiveSubset->vec[inactive] = i;
	  inactive--;
	}
      }
      // F_old=F;
      // F=0.0;
      // for(int i=n; i-- ;){ 
      // 	w[i]+=delta*(w_bar[i]-w[i]);
      // 	F+=w[i]*w[i];
      // }
      // F=0.5*lambda_l*F;      
      // active=0;
      // inactive=m-1;  
      // for(int i=0; i<m ; i++)
      // 	{
      // 	  o[i]+=delta*(o_bar[i]-o[i]);
      // 	  diff=1-Y[i]*o[i];
      // 	  if(diff>0)
      // 	    {
      // 	      ActiveSubset->vec[active]=i;
      // 	      active++;
      // 	      F+=0.5*C[i]*diff*diff;
      // 	    }
      // 	  else
      // 	    {
      // 	      ActiveSubset->vec[inactive]=i;
      // 	      inactive--;
      // 	    }   
      // 	}
      ActiveSubset->d=active;      
      if(fabs(F-F_old)<RELATIVE_STOP_EPS*fabs(F_old))
	{
	  if (verbose > 0)
	    cout << "L2_SVM_MFN converged (rel. criterion) in " << iter << " iterations and "<< tictoc.time() << " seconds. \n" << endl;
	  delete[] ActiveSubset->vec;
	  delete[] ActiveSubset;
	  delete[] o_bar;
	  delete[] w_bar;
	  delete[] Weights_bar;
	  delete[] Outputs_bar;
	  tictoc.stop();
	  return 2;
	}
    }
  delete[] ActiveSubset->vec;
  delete[] ActiveSubset;
  delete[] o_bar;
  delete[] w_bar;
  delete[] Weights_bar;
  delete[] Outputs_bar;
  tictoc.stop();
  if (verbose > 0)
    cout << "L2_SVM_MFN converged (max iter exceeded) in " << iter << " iterations and "<< tictoc.time() << " seconds. \n" << endl;
  return 0;
}

double line_search(double *w, 
                   double *w_bar,
                   double lambda_l,
                   double *o, 
                   double *o_bar, 
                   double *Y, 
                   int d, /* data dimensionality -- 'n' */
                   int l,  double cpos, double cneg) /* number of examples */                  
{                       
  int inc = 1;
  int i = 0;
  double omegaL = 0.0;
  double omegaR = 0.0;
  double diff = 0.0;
  for (int i = d; i--;) {
    diff = w_bar[i] - w[i];
    omegaL += w[i] * diff;
    omegaR += w_bar[i] * diff;
  }
  omegaL = lambda_l * omegaL;
  omegaR = lambda_l * omegaR;
  double L = omegaL;
  double R = omegaR;
  int ii = 0;
  double d2 = 0.0;

  Delta* deltas = new Delta[l];
  int p = 0;
  for (i = 0; i < l; i++) {
    diff = Y[i] * (o_bar[i] - o[i]);
    if (Y[i] * o[i] < 1) {
      d2 = ((Y[i]==1)? cpos : cneg) * (o_bar[i] - o[i]);
      L += (o[i] - Y[i]) * d2;
      R += (o_bar[i] - Y[i]) * d2;
      if (diff > 0) {
        deltas[p].delta = (1 - Y[i] * o[i]) / diff;
        deltas[p].index = i;
        deltas[p].s = -1;
        p++;
      }
    } else {
      if (diff < 0) {
        deltas[p].delta = (1 - Y[i] * o[i]) / diff;
        deltas[p].index = i;
        deltas[p].s = 1;
        p++;
      }
    }
  }
  sort(deltas, deltas + p);
  double delta_prime = 0.0;
  for (i = 0; i < p; i++) {
    delta_prime = L + deltas[i].delta * (R - L);
    if (delta_prime >= 0) {
      break;
    }
    ii = deltas[i].index;
    diff = (deltas[i].s) * ((Y[ii]==1)? cpos : cneg) * (o_bar[ii] - o[ii]);
    L += diff * (o[ii] - Y[ii]);
    R += diff * (o_bar[ii] - Y[ii]);
  }
  delete[] deltas;
  return (-L / (R - L));
} 

/********************** UTILITIES ********************/
double norm_square(const vector_double *A)
{
  double x=0.0, t=0.0;
  for(int i=0;i<A->d;i++)
    {
      t=A->vec[i];
      x+=t*t;
    }
  return x;
} 
void init_vec_double(struct vector_double *A, int k, double a)
{
  double *vec = new double[k];
  for(int i=0;i<k;i++)
    vec[i]=a;
  A->vec = vec;
  A->d   = k;
  return;
}
void init_vec_int(struct vector_int *A, int k)
{  
  int *vec = new int[k];
  for(int i=0;i<k;i++)
    vec[i]=i; 
  A->vec = vec;
  A->d   = k;
  return;
}

void call_L2_SVM_MFN(struct data *Data, 
		     struct options *Options, 
		     struct vector_double *Weights,
		     struct vector_double *Outputs,
		     int verbose, double cpos, double cneg)
{
  // initialize 
  init_vec_double(Weights,Data->n,0.0);
  init_vec_double(Outputs,Data->m,0.0);
  // call L2-SVM-MFn
  int optimality = 0;
  optimality=L2_SVM_MFN(Data,Options,Weights,Outputs,verbose, cpos, cneg);
  return;
} 
void clear_vec_double(struct vector_double *c)
{ delete[] c->vec; return;}
void clear_vec_int(struct vector_int *c)
{ delete[] c->vec; return;}
