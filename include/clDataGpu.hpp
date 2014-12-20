#pragma once

#include <vector>
#include <Eigen/Dense>

#include "gpuMatrix.hpp"
#include "clData.hpp"
#include "timer.hpp"

using namespace Eigen;
using std::vector;

extern void vectorSum_gpu( double *d_x, uint32_t *d_z , uint32_t N, 
    uint32_t k0, uint32_t K, double *d_SSs);
extern void vectorSum_gpu(float *d_x, uint32_t *d_z, 
    uint32_t N, uint32_t k0, uint32_t K, float *d_SSs);

extern void labelMapGpu(uint32_t *d_z, int32_t* d_Map, uint32_t N);

template<typename T>
class ClDataGpu : public ClData<T>
{
protected:
  GpuMatrix<uint32_t> d_z_; // indicators on GPU
  GpuMatrix<T> d_x_; // data-points
  GpuMatrix<T> d_Ss_; // sufficient statistics
 
public: 
  ClDataGpu(const shared_ptr<Matrix<T,Dynamic,Dynamic> >& x, 
      const spVectorXu& z, uint32_t K);
  ClDataGpu(const shared_ptr<Matrix<T,Dynamic,Dynamic> >& x, uint32_t K);
  virtual ~ClDataGpu(){;};

  virtual void labelMap(const vector<int32_t>& map);
  virtual void updateLabels(uint32_t K);
  virtual void computeSS();

  virtual void updateK(uint32_t K){ this->K_ = K;};
  virtual void updateData(const boost::shared_ptr<Matrix<T,Dynamic,Dynamic> >& x);

  virtual VectorXu& z() { this->d_z_.get(this->z_); return ClData<T>::z();};
  virtual T* d_x(){ return d_x_.data();};
  virtual uint32_t* d_z(){ return d_z_.data();};

protected:

  virtual void computeSS(uint32_t k0, uint32_t K);
};

typedef ClDataGpu<double> ClDataGpud;
typedef ClDataGpu<float> ClDataGpuf;

// ------------------------------------ impl --------------------------------
template<typename T>
ClDataGpu<T>::ClDataGpu(const shared_ptr<Matrix<T,Dynamic,Dynamic> >& x, 
    const spVectorXu& z, uint32_t K)
  : ClData<T>(x,z,K), d_z_(this->z_), d_x_(this->x_), d_Ss_((this->D_-1)+1,this->K_)
{cout<<"ClDataGpu constructed"<<endl;};

template<typename T>
ClDataGpu<T>::ClDataGpu(const shared_ptr<Matrix<T,Dynamic,Dynamic> >& x, 
    uint32_t K)
  : ClData<T>(x,K), d_z_(this->z_), d_x_(this->x_), d_Ss_((this->D_-1)+1,this->K_)
{cout<<"ClDataGpu constructed"<<endl;};


template<typename T>
void ClDataGpu<T>::updateLabels(uint32_t K)
{
  this->K_ = K>0?K:this->z_->maxCoeff()+1;
//  assert(this->z_->maxCoeff() < this->K_); // no indicators \geq K
//  assert(this->z_->minCoeff() >= 0); // no indicators \le 0
//  assert((this->z_->array() < this->K_).all());
//  // update the labels on the GPU
//  d_z_.set(this->z_);
}

template<class T>
void ClDataGpu<T>::updateData(const boost::shared_ptr<Matrix<T,Dynamic,Dynamic> >& x)
{
  ClData<T>::updateData(x);
  d_x_.set(this->x_);
  d_z_.set(this->z_);
};

template<class T>
void ClDataGpu<T>::computeSS(uint32_t k0, uint32_t K)
{
//  cout<<"CUDA ::computeSums for k0="<<k0<<" K="<<K<<" N="<<this->N_<<endl;
  Matrix<T,Dynamic,Dynamic> xSums = Matrix<T,Dynamic,Dynamic>::Zero(
      this->D_+1,K);
  GpuMatrix<T> d_xSums(xSums);

//  d_x_.print();
//  d_z_.print();
  vectorSum_gpu(d_x_.data(),d_z_.data(),this->N_,k0,K,d_xSums.data());
  d_xSums.get(xSums); 
//  cout<<xSums<<endl; 
  for (uint32_t k=0; k<K; ++k)
  {
//    this->Ns_(k+k0) = Ss(3,k); //TODO do I need counts?
    this->Ns_(k+k0) = xSums(3,k);
    if(this->Ns_(k+k0) > 0)
      this->xSums_.col(k+k0) = xSums.block(0,k,3,1);
  }
}

template<class T>
void ClDataGpu<T>::computeSS(void)
{
  this->Ns_.setZero(this->K_);
  this->xSums_.setZero(this->D_,this->K_);
  uint32_t k0 = 0;
  if(this->K_ <= 6)
  {
    computeSS(0,this->K_); // max 6 SSs per kernel due to shared mem
  }else{
    for (k0=0; k0<this->K_; k0+=6)
      computeSS(k0,min(this->K_-k0,uint32_t(6))); // max 6 SSs per kernel due to shared mem
  }
};

template<class T>
void ClDataGpu<T>::labelMap(const vector<int32_t>& map)
{
  GpuMatrix<int32_t> d_map(map);
//  cout<<"GPU labelMap: "<<d_map.get().transpose()<<endl;
//  VectorXu& z = this->z();
//  cout<<"z min/max: "<<z.maxCoeff()<<" "<<z.minCoeff()<<endl;
  // fix labels
  labelMapGpu(d_z_.data(),d_map.data(),this->N_);  
//  z = this->z();
//  cout<<"z min/max: "<<z.maxCoeff()<<" "<<z.minCoeff()<<endl;
};
