

#include <functional>
#include <utility>
#include <list>

using std::list;
using std::pair;


#include "eigenIncluder.hpp"
#include "algebraTrace.hpp"
#include "streamTrace.hpp"
#include "testUtils.hpp"
#include "algebra.hpp"
#include "common.hpp"
#include "acsQC.hpp"

/* new matrix ------------------------------------------------------------------
* allocate memory of matrix
* args   : int    n,m       I   number of rows and columns of matrix
* return : matrix pointer (if n<=0 or m<=0, return NULL)
*-----------------------------------------------------------------------------*/
[[deprecated]]
extern double *mat(int n, int m)
{
	double *p;

	if (n<=0||m<=0) return NULL;
	if (!(p=(double *)malloc(sizeof(double)*n*m))) {
		fatalerr("matrix memory allocation error: n=%d,m=%d\n",n,m);
	}
	return p;
}
/* new integer matrix ----------------------------------------------------------
* allocate memory of integer matrix
* args   : int    n,m       I   number of rows and columns of matrix
* return : matrix pointer (if n<=0 or m<=0, return NULL)
*-----------------------------------------------------------------------------*/
[[deprecated]]
extern int *imat(int n, int m)
{
	int *p;

	if (n<=0||m<=0) return NULL;
	if (!(p=(int *)malloc(sizeof(int)*n*m))) {
		fatalerr("integer matrix memory allocation error: n=%d,m=%d\n",n,m);
	}
	return p;
}
/* zero matrix -----------------------------------------------------------------
* generate new zero matrix
* args   : int    n,m       I   number of rows and columns of matrix
* return : matrix pointer (if n<=0 or m<=0, return NULL)
*-----------------------------------------------------------------------------*/
[[deprecated]]
extern double *zeros(int n, int m)
{
	double *p;

#if NOCALLOC
	if ((p=mat(n,m))) for (n=n*m-1;n>=0;n--) p[n]=0.0;
#else
	if (n<=0||m<=0) return NULL;
	if (!(p=(double *)calloc(sizeof(double),n*m))) {
		fatalerr("matrix memory allocation error: n=%d,m=%d\n",n,m);
	}
#endif
	return p;
}
/* identity matrix -------------------------------------------------------------
* generate new identity matrix
* args   : int    n         I   number of rows and columns of matrix
* return : matrix pointer (if n<=0, return NULL)
*-----------------------------------------------------------------------------*/
[[deprecated]]
extern double *eye(int n)
{
	double *p;
	int i;

	if ((p=zeros(n,n))) for (i=0;i<n;i++) p[i+i*n]=1.0;
	return p;
}
/* inner product ---------------------------------------------------------------
* inner product of vectors
* args   : double *a,*b     I   vector a,b (n x 1)
*          int    n         I   size of vector a,b
* return : a'*b
*-----------------------------------------------------------------------------*/
[[deprecated]]
extern double dot(const double *a, const double *b, int n)
{
	double c=0.0;

	while (--n>=0) c+=a[n]*b[n];
	return c;
}
/* euclid norm -----------------------------------------------------------------
* euclid norm of vector
* args   : double *a        I   vector a (n x 1)
*          int    n         I   size of vector a
* return : || a ||
*-----------------------------------------------------------------------------*/
[[deprecated]]
extern double norm(const double *a, int n)
{
	return sqrt(dot(a,a,n));
}
/* copy matrix -----------------------------------------------------------------
* copy matrix
* args   : double *A        O   destination matrix A (n x m)
*          double *B        I   source matrix B (n x m)
*          int    n,m       I   number of rows and columns of matrix
* return : none
*-----------------------------------------------------------------------------*/
[[deprecated]]
extern void matcpy(double *A, const double *B, int n, int m)
{
	memcpy(A,B,sizeof(double)*n*m);
}
/* matrix routines -----------------------------------------------------------*/
#ifdef LAPACK /* with LAPACK/BLAS or MKL */

/* multiply matrix (wrapper of blas dgemm) -------------------------------------
* multiply matrix by matrix (C=alpha*A*B+beta*C)
* args   : char   *tr       I  transpose flags ("N":normal,"T":transpose)
*          int    n,k,m     I  size of (transposed) matrix A,B
*          double alpha     I  alpha
*          double *A,*B     I  (transposed) matrix A (n x m), B (m x k)
*          double beta      I  beta
*          double *C        IO matrix C (n x k)
* return : none
*-----------------------------------------------------------------------------*/
[[deprecated]]
extern void matmul(const char *tr, int n, int k, int m, double alpha,
				const double *A, const double *B, double beta, double *C)
{
	int lda=tr[0]=='T'?m:n,ldb=tr[1]=='T'?k:m;

	dgemm_((char *)tr,(char *)tr+1,&n,&k,&m,&alpha,(double *)A,&lda,(double *)B,
		&ldb,&beta,C,&n);
}
/* inverse of matrix -----------------------------------------------------------
* inverse of matrix (A=A^-1)
* args   : double *A        IO  matrix (n x n)
*          int    n         I   size of matrix A
* return : status (0:ok,0>:error)
*-----------------------------------------------------------------------------*/
[[deprecated]]
extern int matinv(double *A, int n)
{
	double *work;
	int info,lwork=n*16,*ipiv=imat(n,1);

	work=mat(lwork,1);
	dgetrf_(&n,&n,A,&n,ipiv,&info);
	if (!info) dgetri_(&n,A,&n,ipiv,work,&lwork,&info);
	free(ipiv); free(work);
	return info;
}
/* solve linear equation -------------------------------------------------------
* solve linear equation (X=A\Y or X=A'\Y)
* args   : char   *tr       I   transpose flag ("N":normal,"T":transpose)
*          double *A        I   input matrix A (n x n)
*          double *Y        I   input matrix Y (n x m)
*          int    n,m       I   size of matrix A,Y
*          double *X        O   X=A\Y or X=A'\Y (n x m)
* return : status (0:ok,0>:error)
* notes  : matirix stored by column-major order (fortran convention)
*          X can be same as Y
*-----------------------------------------------------------------------------*/
[[deprecated]]
extern int solve(const char *tr, const double *A, const double *Y, int n,
				int m, double *X)
{
	double *B=mat(n,n);
	int info,*ipiv=imat(n,1);

	matcpy(B,A,n,n);
	matcpy(X,Y,n,m);
	dgetrf_(&n,&n,B,&n,ipiv,&info);
	if (!info) dgetrs_((char *)tr,&n,&m,B,&n,ipiv,X,&n,&info);
	free(ipiv); free(B);
	return info;
}

#else /* without LAPACK/BLAS or MKL */

/* multiply matrix -----------------------------------------------------------*/
[[deprecated]]
extern void matmul(const char *tr, int n, int k, int m, double alpha,
				const double *A, const double *B, double beta, double *C)
{
	double d;
	int i,j,x,f=tr[0]=='N'?(tr[1]=='N'?1:2):(tr[1]=='N'?3:4);

	for (i=0;i<n;i++) for (j=0;j<k;j++) {
		d=0.0;
		switch (f) {
			case 1: for (x=0;x<m;x++) d+=A[i+x*n]*B[x+j*m]; break;
			case 2: for (x=0;x<m;x++) d+=A[i+x*n]*B[j+x*k]; break;
			case 3: for (x=0;x<m;x++) d+=A[x+i*m]*B[x+j*m]; break;
			case 4: for (x=0;x<m;x++) d+=A[x+i*m]*B[j+x*k]; break;
		}
		if (beta==0.0) C[i+j*n]=alpha*d; else C[i+j*n]=alpha*d+beta*C[i+j*n];
	}
}
/* LU decomposition ----------------------------------------------------------*/
[[deprecated]]
int ludcmp(double *A, int n, int *indx, double *d)
{
	double big,s,tmp,*vv=mat(n,1);
	int i,imax=0,j,k;

	*d=1.0;
	for (i=0;i<n;i++) {
		big=0.0; for (j=0;j<n;j++) if ((tmp=fabs(A[i+j*n]))>big) big=tmp;
		if (big>0.0) vv[i]=1.0/big; else {free(vv); return -1;}
	}
	for (j=0;j<n;j++) {
		for (i=0;i<j;i++) {
			s=A[i+j*n]; for (k=0;k<i;k++) s-=A[i+k*n]*A[k+j*n]; A[i+j*n]=s;
		}
		big=0.0;
		for (i=j;i<n;i++) {
			s=A[i+j*n]; for (k=0;k<j;k++) s-=A[i+k*n]*A[k+j*n]; A[i+j*n]=s;
			if ((tmp=vv[i]*fabs(s))>=big) {big=tmp; imax=i;}
		}
		if (j!=imax) {
			for (k=0;k<n;k++) {
				tmp=A[imax+k*n]; A[imax+k*n]=A[j+k*n]; A[j+k*n]=tmp;
			}
			*d=-(*d); vv[imax]=vv[j];
		}
		indx[j]=imax;
		if (A[j+j*n]==0.0) {free(vv); return -1;}
		if (j!=n-1) {
			tmp=1.0/A[j+j*n]; for (i=j+1;i<n;i++) A[i+j*n]*=tmp;
		}
	}
	free(vv);
	return 0;
}
/* LU back-substitution ------------------------------------------------------*/
[[deprecated]]
void lubksb(const double *A, int n, const int *indx, double *b)
{
	double s;
	int i,ii=-1,ip,j;

	for (i=0;i<n;i++) {
		ip=indx[i]; s=b[ip]; b[ip]=b[i];
		if (ii>=0) for (j=ii;j<i;j++) s-=A[i+j*n]*b[j]; else if (s) ii=i;
		b[i]=s;
	}
	for (i=n-1;i>=0;i--) {
		s=b[i]; for (j=i+1;j<n;j++) s-=A[i+j*n]*b[j]; b[i]=s/A[i+i*n];
	}
}
/* inverse of matrix ---------------------------------------------------------*/
[[deprecated]]
extern int matinv(double *A, int n)
{
	double d,*B;
	int i,j,*indx;

	indx=imat(n,1); B=mat(n,n); matcpy(B,A,n,n);
	if (ludcmp(B,n,indx,&d)) {free(indx); free(B); return -1;}
	for (j=0;j<n;j++) {
		for (i=0;i<n;i++) A[i+j*n]=0.0; A[j+j*n]=1.0;
		lubksb(B,n,indx,A+j*n);
	}
	free(indx); free(B);
	return 0;
}
/* solve linear equation -----------------------------------------------------*/
[[deprecated]]
extern int solve(const char *tr, const double *A, const double *Y, int n,
				int m, double *X)
{
	double *B=mat(n,n);
	int info;

	matcpy(B,A,n,n);
	if (!(info=matinv(B,n))) matmul(tr[0]=='N'?"NN":"TN",n,m,n,1.0,B,Y,0.0,X);
	free(B);
	return info;
}
#endif
/* end of matrix routines ----------------------------------------------------*/

/* least square estimation -----------------------------------------------------
* least square estimation by solving normal equation (x=(A*A')^-1*A*y)
* args   : double *A        I   transpose of (weighted) design matrix (n x m)
*          double *y        I   (weighted) measurements (m x 1)
*          int    n,m       I   number of parameters and measurements (n<=m)
*          double *x        O   estmated parameters (n x 1)
*          double *Q        O   esimated parameters covariance matrix (n x n)
* return : status (0:ok,0>:error)
* notes  : for weighted least square, replace A and y by A*w and w*y (w=W^(1/2))
*          matirix stored by column-major order (fortran convention)
*-----------------------------------------------------------------------------*/
extern int lsq(const double *A, const double *y, int n, int m, double *x,
			double *Q)
{
	double *Ay;
	int info;

	if (m<n) return -1;
	Ay=mat(n,1);
	matmul("NN",n,1,m,1.0,A,y,0.0,Ay); /* Ay=A*y */
	matmul("NT",n,n,m,1.0,A,A,0.0,Q);  /* Q=A*A' */
	if (!(info=matinv(Q,n))) matmul("NN",n,1,n,1.0,Q,Ay,0.0,x); /* x=Q^-1*Ay */
	free(Ay);
	return info;
}

/* kalman filter ---------------------------------------------------------------
* kalman filter state update as follows:
*
*   K=P*H*(H'*P*H+R)^-1, xp=x+K*v, Pp=(I-K*H')*P
*
* args   : double *x        I   states vector (n x 1)
*          double *P        I   covariance matrix of states (n x n)
*          double *H        I   transpose of design matrix (n x m)
*          double *v        I   innovation (measurement - model) (m x 1)
*          double *R        I   covariance matrix of measurement error (m x m)
*          int    n,m       I   number of states and measurements
*          double *xp       O   states vector after update (n x 1)
*          double *Pp       O   covariance matrix of states after update (n x n)
* return : status (0:ok,<0:error)
* notes  : matirix stored by column-major order (fortran convention)
*          if state x[i]==0.0, not updates state x[i]/P[i+i*n]
*-----------------------------------------------------------------------------*/
[[deprecated]]
extern int filter_(const double *x, const double *P, const double *H,
				const double *v, const double *R, int n, int m,
				double *xp, double *Pp)
{
	double *F=mat(n,m),*Q=mat(m,m),*K=mat(n,m),*I=eye(n);
	int info;

	matcpy(Q,R,m,m);
	matcpy(xp,x,n,1);
	matmul("NN",n,m,n,1.0,P,H,0.0,F);       /* Q=H'*P*H+R */
	matmul("TN",m,m,n,1.0,H,F,1.0,Q);
	if (!(info=matinv(Q,m))) {
		matmul("NN",n,m,m,1.0,F,Q,0.0,K);   /* K=P*H*Q^-1 */
		matmul("NN",n,1,m,1.0,K,v,1.0,xp);  /* xp=x+K*v */
		matmul("NT",n,n,m,-1.0,K,H,1.0,I);  /* Pp=(I-K*H')*P */
		matmul("NN",n,n,n,1.0,I,P,0.0,Pp);
	}
	free(F); free(Q); free(K); free(I);
	return info;
}

[[deprecated]]
extern int filter(double *x, double *P, const double *H, const double *v,
				const double *R, int n, int m)
{
	double *x_,*xp_,*P_,*Pp_,*H_;
	int i,j,k,info,*ix;

	ix=imat(n,1); for (i=k=0;i<n;i++) if (x[i]!=0.0&&P[i+i*n]>0.0) ix[k++]=i;
	x_=mat(k,1); xp_=mat(k,1); P_=mat(k,k); Pp_=mat(k,k); H_=mat(k,m);
	for (i=0;i<k;i++) {
		x_[i]=x[ix[i]];
		for (j=0;j<k;j++) P_[i+j*k]=P[ix[i]+ix[j]*n];
		for (j=0;j<m;j++) H_[i+j*k]=H[ix[i]+j*n];
	}
	info=filter_(x_,P_,H_,v,R,k,m,xp_,Pp_);
	for (i=0;i<k;i++) {
		x[ix[i]]=xp_[i];
		for (j=0;j<k;j++) P[ix[i]+ix[j]*n]=Pp_[i+j*k];
	}
	free(ix); free(x_); free(xp_); free(P_); free(Pp_); free(H_);
	return info;
}


/* least-squares and quality control by chi-square testing  --------------------
* args     :       file   *fp              I       output file
*                  double *H               I       design matrix (mxn)
*                  double *P               I       weight matrix (mxm)
*                  double *Z               I       observed minus computed (mx1)
*                  double *xo              O       estimates (nx1)
*                  double *Po              O       vc-matrix of estimates (nxn)
*                  int m                   I       number of observations
*                  int n                   I       number of unknowns (always 2)
*
* return   :       0 - no cycle slip, 1 - cycle slip detected
*
* ref [2], weighted ionosphere model
* ---------------------------------------------------------------------------*/
extern int lsqqc(
	Trace&	trace,
	const double *H,
	const double *P,
	const double *Z,
	double *v,
	double *xo,
	double *Po,
	int m,
	int n,
	int ind,
	int norb)
{
	double *xp, *N, *N1, *Pp, *vtp, *L, *g, *S;
	int info = 0, i;

	/* memory allocation */
	xp = mat(n, 1);
	N = mat(n, m);
	Pp = mat(n, n);
	N1 = mat(n, 1);
	vtp = mat(1, m);
	L = mat(n, n);
	g = mat(n, 1);

	S = zeros(n, n);

	/* least-squares */
	matmul("TN", n, m, m, 1.0, H, P, 0.0, N); /* H'*P */
	matmul("NN", n, n, m, 1.0, N, H, 0.0, Pp); /* H'*P*H */
	matmul("NN", n, 1, m, 1.0, N, Z, 0.0, N1); /* Nl=H'*P*Z */

	//TODO build constraint matrix about here
	/* constrain the 1st epoch LS orbit estimation, to be refined */
	if (norb > 0)
	{
		for (i = 0; i < norb; i++)
		{
			Pp[i + i * n] += 1E6;
		}
	}

	matcpy(L, Pp, n, n);

	if (!matinv(Pp, n))
	{
		matmul("NN", n, 1, n, 1.0, Pp, N1, 0.0, xp);

		/* chi-square testing */
		info = chiqc(trace, H, P, Z, xp, v, m, n, ind);

		/* for output */
		if (xo != NULL)
			matcpy(xo, xp, n, 1);
		if (Po != NULL)
			matcpy(Po, Pp, n, n);
	}
	else
	{
		info = 1;
		tracepdeex(1, trace, "vtpv= Warning: least-squares estimation error\n");
	}

	free(xp);
	free(N);
	free(N1);
	free(Pp);
	free(vtp);
	free(L);
	free(g);
	free(S);
	return info;
}

/* quality control using chi-square test ---------------------------------------
* args     :       File   *fp              I       output file
*                  const double *H         I       design matrix (mxn)
*                  const double *P         I       weight matrix (mxm)
*                  const double *Z         I       observed - computed (mx1)
*                  const double *xp        I       estimated parameters (nx1)
*                  double *v               O       post-fit residual (mx1)
*                  int m                   I       number of observations
*                  int n                   I       number of unknowns
*
* return   :       0 - no outlier, 1 - outlier detected
*
* ref [2], weighted ionosphere model
* ---------------------------------------------------------------------------*/
extern int chiqc(
	Trace&	trace,
	const double *H,
	const double *P,
	const double *Z,
	const double *xp,
	double *v,
	int m,
	int n,
	int ind)
{
	int info = 0;
	double *vtp = mat(1, m), vtpv = 0.0, val, thres;

	matcpy(v, Z, m, 1);

	/* calculate vtpv for chi-square testing */
	matmul("NN", m, 1, n, 1.0, H, xp, -1.0, v); /* v = H*xp-v */
	matmul("TN", 1, m, m, 1.0, v, P, 0.0, vtp); /* vtpv */
	matmul("NN", 1, 1, m, 1.0, vtp, v, 0.0, &vtpv);

	if (ind == 0)
	{
		val = vtpv / (m - n);
#if (1)
		thres = chisqr[m - n - 1] / (m - n);
#else
		thres=3.0;
#endif
	}
	else
	{
		val = vtpv / m;
		thres = 35.0;
	}

	tracepdeex(2, trace, "     vtpv=%8.1f val=%8.1f thres=%6.2f %4d %4d", vtpv, val, thres, m, n);
	//TODO change this to boost info
	//printf("     vtpv=%8.1f val=%8.1f thres=%6.2f %4d %4d\n",vtpv,val,thres,m,n);
	/* chi-square validation */
	if (val > thres)
	{
		tracepdeex(1, trace, " detected by LOM");	//Local Overall Model
		info = 1;
	}

	free(vtp);

	return info;
}

