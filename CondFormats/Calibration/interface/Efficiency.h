#ifdef CondEx_Efficiency_H
#define CondEx_Efficiency_H
/*  example of polymorphic condition
 *  LUT, function, mixed....
 */

#include<cmath>

namespace condex {
  /* very simple base class
   * trivial inheritance, no template tricks 
   */
  class Efficiency {
  public:
    virtual ~Efficiency(){}
    float operator()(float pt, float eta) const {
      return value(pt,eta);
    }

    virtual float value(float pt, float eta) const=0;

  };

  class ParametricEfficiencyInPt : public Efficiency {
  public:
    ParametricEfficiencyInPt(float cm, float ch,
			    float el, eh) :
      cutLow(cm), cutHigh(ch),
      low(el), high(eh){}
  private:
    virtual float value(float pt, float) const {
      if ( pt<low) return cutLow;
      if ( pt>high) return cutHigh;
      return cutLow + (pt-low)/(high-low)*(cutHigh-cutLow);
    }
    float cutLow, cutHigh;
    float low, high;

  };  

class ParametricEfficiencyInEta : public Efficiency {
  public:
    ParametricEfficiencyInEta(float cmin, float cmax,
			    float el, eh) :
      cutLow(cmin), cutHigh(cmax),
      low(el), high(eh){}
  private:
    virtual float value(float, float eta) const {
      eta = std::abs(eta);
      if ( eta<low) return cutLow;
      if ( eta>high) return cutHigh;
      return cutLow + (pt-low)/(high-low)*(cutHigh-cutLow);
    }
    float cutLow, cutHigh;
    float low, high;

  };

}




#endif
