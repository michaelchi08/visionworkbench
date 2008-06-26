// __BEGIN_LICENSE__
// 
// Copyright (C) 2006 United States Government as represented by the
// Administrator of the National Aeronautics and Space Administration
// (NASA).  All Rights Reserved.
// 
// Copyright 2006 Carnegie Mellon University. All rights reserved.
// 
// This software is distributed under the NASA Open Source Agreement
// (NOSA), version 1.3.  The NOSA has been approved by the Open Source
// Initiative.  See the file COPYING at the top of the distribution
// directory tree for the complete NOSA document.
// 
// THE SUBJECT SOFTWARE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY OF ANY
// KIND, EITHER EXPRESSED, IMPLIED, OR STATUTORY, INCLUDING, BUT NOT
// LIMITED TO, ANY WARRANTY THAT THE SUBJECT SOFTWARE WILL CONFORM TO
// SPECIFICATIONS, ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR
// A PARTICULAR PURPOSE, OR FREEDOM FROM INFRINGEMENT, ANY WARRANTY THAT
// THE SUBJECT SOFTWARE WILL BE ERROR FREE, OR ANY WARRANTY THAT
// DOCUMENTATION, IF PROVIDED, WILL CONFORM TO THE SUBJECT SOFTWARE.
// 
// __END_LICENSE__

/// \file Convolution.h
/// 
/// One- and two-dimensional convolution numerical functions, and 
/// standard and separable two-dimensional image convolution view 
/// classes used by the filtering functions in \ref Filter.h.
///
#ifndef __VW_IMAGE_CONVOLUTION_H__
#define __VW_IMAGE_CONVOLUTION_H__

#include <vector>
#include <iterator>

#include <vw/Core/Exception.h>
#include <vw/Core/TypeDeduction.h>
#include <vw/Image/ImageView.h>
#include <vw/Image/Manipulation.h>
#include <vw/Image/EdgeExtension.h>
#include <vw/Image/PixelMask.h>

namespace vw {

  // *******************************************************************
  // The core correlation functions
  // *******************************************************************

  /// \cond INTERNAL

  // These primitive operations are currently only used by the
  // convolution views, but I kept them free functions because I
  // thought they might be useful down the road in other
  // correlation-type functions....

  template <class SrcAccessT, class KernelIterT>
  typename ProductType<typename SrcAccessT::pixel_type, typename std::iterator_traits<KernelIterT>::value_type>::type
  inline correlate_1d_at_point( SrcAccessT const& src, KernelIterT const& kernel, int32 n ) {
    typedef typename ProductType<typename SrcAccessT::pixel_type, typename std::iterator_traits<KernelIterT>::value_type>::type result_type;
    result_type result = result_type();
    validate(result);
    SrcAccessT s = src;
    KernelIterT k = kernel;
    for( int32 i=n; i; --i ) {
      result += (*k)*(*s);
      s.next_col();
      ++k;
    }
    return result;
  }

  template <class SrcAccessT, class KernelAccessT>
  typename ProductType<typename SrcAccessT::pixel_type, typename KernelAccessT::pixel_type>::type
  inline correlate_2d_at_point( SrcAccessT const& src, KernelAccessT const& kernel, int32 cols, int32 rows ) {
    typedef typename ProductType<typename SrcAccessT::pixel_type, typename KernelAccessT::pixel_type>::type result_type;

    result_type result = result_type();
    validate(result);
    SrcAccessT srow = src;
    KernelAccessT krow = kernel;
    for( int32 j=rows; j; --j ) {
      SrcAccessT scol = srow;
      KernelAccessT kcol = krow;
      for( int32 i=cols; i; --i ) {
        result += (*kcol)*(*scol);
        scol.next_col();
        kcol.next_col();
      }
      srow.next_row();
      krow.next_row();
    }
    return result;
  }

  /// \endcond


  // *******************************************************************
  // The standard 2D convolution view type
  // *******************************************************************

  /// A standard 2D convolution image view.
  ///
  /// Represents the convolution of an image with a 2D kernel.
  ///
  /// \see ConvolutionFilter
  template <class ImageT, class KernelT, class EdgeT>
  class ConvolutionView : public ImageViewBase<ConvolutionView<ImageT,KernelT,EdgeT> >
  {
  private:
    ImageT m_image;
    EdgeT m_edge;
    Rotate180View<KernelT> m_kernel;
    int32 m_ci, m_cj;

  public:
    /// The pixel type of the image view.
    typedef typename ProductType<typename ImageT::pixel_type, typename KernelT::pixel_type>::type pixel_type;

    /// We compute the result, so we return by value.
    typedef pixel_type result_type;

    /// The view's %pixel_accessor type.
    typedef ProceduralPixelAccessor<ConvolutionView<ImageT, KernelT, EdgeT> > pixel_accessor;

    /// Constructs a ConvolutionView with the given image and kernel and with the origin of the kernel located at the point (ci,cj).
    ConvolutionView( ImageT const& image, KernelT const& kernel, int32 ci, int32 cj, EdgeT const& edge = EdgeT() )
      : m_image(image), m_edge(edge), m_kernel(kernel), m_ci(ci), m_cj(cj) {}

    /// Constructs a ConvolutionView with the given image and kernel and with the origin of the kernel located at the center.
    ConvolutionView( ImageT const& image, KernelT const& kernel, EdgeT const& edge = EdgeT() )
      : m_image(image), m_edge(edge), m_kernel(kernel), m_ci((kernel.cols()-1)/2), m_cj((kernel.rows()-1)/2) {}

    /// Returns the number of columns in the image.
    inline int32 cols() const { return m_image.cols(); }

    /// Returns the number of rows in the image.
    inline int32 rows() const { return m_image.rows(); }

    /// Returns the number of planes in the image.
    inline int32 planes() const { return m_image.planes(); }

    /// Returns a pixel_accessor pointing to the origin.
    inline pixel_accessor origin() const { return pixel_accessor( *this ); }

    /// Returns the pixel at the given position in the given plane.
    inline result_type operator()( int32 x, int32 y, int32 p=0 ) const {
      int32 ci = (m_kernel.cols()-1-m_ci), cj = (m_kernel.rows()-1-m_cj);
      if( (x >= ci) && (y >= cj) &&
	  (x <= int(m_image.cols())-int(m_kernel.cols())+ci) &&
	  (y <= int(m_image.rows())-int(m_kernel.rows())+cj) ) {
        return correlate_2d_at_point( m_image.origin().advance(x-ci,y-cj,p),
                                      m_kernel.origin(), m_kernel.cols(), m_kernel.rows() );
      }
      else {
	return correlate_2d_at_point( edge_extend(m_image,m_edge).origin().advance(x-ci,y-cj,p),
                                      m_kernel.origin(), m_kernel.cols(), m_kernel.rows() );
      }
    }

    typedef ConvolutionView<CropView<ImageView<typename ImageT::pixel_type> >, KernelT, NoEdgeExtension> prerasterize_type;
    
    inline prerasterize_type prerasterize( BBox2i bbox ) const {
      int32 ci = (m_kernel.cols()-1-m_ci), cj = (m_kernel.rows()-1-m_cj);
      BBox2i src_bbox( bbox.min().x() - ci, bbox.min().y() - cj,
                       bbox.width() + (m_kernel.cols()-1), bbox.height() + (m_kernel.rows()-1) );
      ImageView<typename ImageT::pixel_type> src = edge_extend( m_image, src_bbox, m_edge );
      return prerasterize_type( crop( src, -src_bbox.min().x(), -src_bbox.min().y(), m_image.cols(), m_image.rows() ),
                                m_kernel.child(), m_ci, m_cj, NoEdgeExtension() );
    }

    template <class DestT> inline void rasterize( DestT const& dest, BBox2i bbox ) const {
      vw::rasterize( prerasterize(bbox), dest, bbox );
    }
  };


  // *******************************************************************
  // The separable 2D convolution view type
  // *******************************************************************

  /// A separable 2D convolution view.
  ///
  /// Represents the convolution of an image with a separable 2D
  /// kernel, specified in terms of its x and y factors.
  template <class ImageT, class KernelT, class EdgeT >
  class SeparableConvolutionView : public ImageViewBase<SeparableConvolutionView<ImageT,KernelT,EdgeT> >
  {
  private:
    ImageT m_image;
    std::vector<KernelT> m_i_kernel, m_j_kernel;
    int32 m_ci, m_cj;
    EdgeT m_edge;
    mutable ImageView<KernelT> m_kernel2d;

    void generate2DKernel() const {
      int32 ni = m_i_kernel.size() ? m_i_kernel.size() : 1;
      int32 nj = m_j_kernel.size() ? m_j_kernel.size() : 1;
      m_kernel2d.set_size( ni, nj );
      for( int32 i=0; i<ni; ++i )
        for( int32 j=0; j<nj; ++j )
          m_kernel2d(ni-1-i,nj-1-j) = (m_i_kernel.size()?m_i_kernel[i]:1)*(m_j_kernel.size()?m_j_kernel[j]:1);
    }

  public:
    /// The pixel type of the view.
    typedef typename ProductType<typename ImageT::pixel_type, KernelT>::type pixel_type;
    typedef pixel_type result_type;

    /// The view's %pixel_accessor type.
    typedef ProceduralPixelAccessor<SeparableConvolutionView<ImageT, KernelT, EdgeT> > pixel_accessor;

    /// Constructs a SeparableConvolutionView with the given image and kernels and with the origin of the kernel located at the center.
    template <class KRangeT>
    SeparableConvolutionView( ImageT const& image, KRangeT const& ik, KRangeT const& jk, int32 ci, int32 cj, EdgeT const& edge = EdgeT() ) : 
      m_image(image), m_i_kernel(ik.begin(),ik.end()), m_j_kernel(jk.begin(),jk.end()), m_ci(ci), m_cj(cj), m_edge(edge) {}

    /// Constructs a SeparableConvolutionView with the given image and kernels and with the origin of the kernel located at the point (ci,cj).
    template <class KRangeT>
    SeparableConvolutionView( ImageT const& image, KRangeT const& ik, KRangeT const& jk, EdgeT const& edge = EdgeT() ) :
      m_image(image), m_i_kernel(ik.begin(),ik.end()), m_j_kernel(jk.begin(),jk.end()), m_ci((m_i_kernel.size()-1)/2), m_cj((m_j_kernel.size()-1)/2), m_edge(edge) {}

    /// Returns the number of columns in the image.
    inline int32 cols() const { return m_image.cols(); }

    /// Returns the number of rows in the image.
    inline int32 rows() const { return m_image.rows(); }

    /// Returns the number of planes in the image.
    inline int32 planes() const { return m_image.planes(); }

    /// Returns a pixel_accessor pointing to the top-left corner of the first plane.
    inline pixel_accessor origin() const { return pixel_accessor( *this ); }

    /// Returns the pixel at the given position in the given plane.
    inline result_type operator()( int32 x, int32 y, int32 p=0 ) const {
      if( m_kernel2d.cols()==0 ) generate2DKernel();
      int32 ci = m_i_kernel.size() ? (m_i_kernel.size()-1-m_ci) : 0;
      int32 cj = m_j_kernel.size() ? (m_j_kernel.size()-1-m_cj) : 0;
      if( (x >= int(ci)) && (y >= int(cj)) &&
          (x <= int(m_image.cols())-int(m_kernel2d.cols())+int(ci)) &&
          (y <= int(m_image.rows())-int(m_kernel2d.rows())+int(cj)) ) {
        return correlate_2d_at_point( m_image.origin().advance(x-ci,y-cj,p),
                                      m_kernel2d.origin(), m_kernel2d.cols(), m_kernel2d.rows() );
      }
      else {
        return correlate_2d_at_point( edge_extend(m_image,m_edge).origin().advance(x-ci,y-cj,p),
                                      m_kernel2d.origin(), m_kernel2d.cols(), m_kernel2d.rows() );
      }
    }

    /// \cond INTERNAL

    // The separable convolution view knows that it is fastest to
    // fully rasterize itself if it is about to be part of a
    // rasterization operation with nested views.  This is actually 
    // not the most efficient behavior: it need only rasterize one 
    // of the two axes, and none at all if only one axis is active. 
    // However, that is deterimined at run time and would impact 
    // the prerasterize_type, so we cannot easily do that.
    typedef ImageView<pixel_type> prerasterize_type;
    inline prerasterize_type prerasterize( BBox2i bbox ) const {
      ImageView<pixel_type> dest( bbox.width(), bbox.height(), m_image.planes() );
      rasterize( dest, bbox );
      return dest;
    }
    
    // In principle we could avoid rasterizing the child first if it's 
    // MultiplyAccessible.  In practice that turns out to be a pain to 
    // get right, and convolution is generally a much more expensive 
    // operation than a single extra copy.
    template <class DestT>
    void rasterize( DestT const& dest, BBox2i bbox ) const {
      int32 ni = m_i_kernel.size(), nj = m_j_kernel.size();
      if( ni==0 && nj==0 ) {
        return edge_extend(m_image,m_edge).rasterize(dest,bbox);
      }
      BBox2i child_bbox = bbox;
      child_bbox.min() -= Vector2i( ni?(ni-m_ci-1):0, nj?(nj-m_cj-1):0 );
      child_bbox.max() += Vector2i( ni?m_ci:0, nj?m_cj:0 );
      ImageView<typename ImageT::pixel_type> src_buf = edge_extend(m_image,child_bbox,m_edge);
      if( ni>0 && nj>0 ) {
        ImageView<pixel_type> work( bbox.width(), child_bbox.height() );
        convolve_1d( src_buf, work, m_i_kernel );
        src_buf.reset(); // Free up some memory
        convolve_1d( transpose(work), transpose(dest), m_j_kernel );
      }
      else if( ni>0 ) {
        convolve_1d( src_buf, dest, m_i_kernel );
      }
      else /* nj>0 */ {
        convolve_1d( transpose(src_buf), transpose(dest), m_j_kernel );
      }
    }

    template <class SrcT, class DestT>
    void convolve_1d( SrcT const& src, DestT const& dest, std::vector<KernelT> const& kernel ) const {
      typedef typename SrcT::pixel_accessor SrcAccessT;
      typedef typename DestT::pixel_accessor DestAccessT;
      typedef typename DestT::pixel_type DestPixelT;

      SrcAccessT splane = src.origin();
      DestAccessT dplane = dest.origin();
      for( int32 p=0; p<src.planes(); ++p ) {
        SrcAccessT srow = splane;
        DestAccessT drow = dplane;
        for( int32 y=0; y<dest.rows(); ++y ) {
          SrcAccessT scol = srow;
          DestAccessT dcol = drow;
          for( int32 x=0; x<dest.cols(); ++x ) {
            *dcol = DestPixelT( correlate_1d_at_point( scol, kernel.rbegin(), kernel.size() ) );
            scol.next_col();
            dcol.next_col();
          }
          srow.next_row();
          drow.next_row();
        }
        splane.next_plane();
        dplane.next_plane();
      }
    }
    
    /// \endcond
  };

} // namespace vw

#endif // __VW_IMAGE_CONVOLUTION_H__
