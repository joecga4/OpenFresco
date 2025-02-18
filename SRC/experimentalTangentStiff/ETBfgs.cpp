/* ****************************************************************** **
**    OpenFRESCO - Open Framework                                     **
**                 for Experimental Setup and Control                 **
**                                                                    **
**                                                                    **
** Copyright (c) 2006, The Regents of the University of California    **
** All Rights Reserved.                                               **
**                                                                    **
** Commercial use of this program without express permission of the   **
** University of California, Berkeley, is strictly prohibited. See    **
** file 'COPYRIGHT_UCB' in main directory for information on usage    **
** and redistribution, and for a DISCLAIMER OF ALL WARRANTIES.        **
**                                                                    **
** Developed by:                                                      **
**   Andreas Schellenberg (andreas.schellenberg@gmx.net)              **
**   Yoshikazu Takahashi (yos@catfish.dpri.kyoto-u.ac.jp)             **
**   Gregory L. Fenves (fenves@berkeley.edu)                          **
**   Stephen A. Mahin (mahin@berkeley.edu)                            **
**                                                                    **
** ****************************************************************** */

// Written: Hong Kim (hongkim@berkeley.edu)
// Created: 10/10
// Revision: A
//
// Description: This file contains the class definition for 
// ETBfgs.  This class uses the measured displacement vector and 
// resisting for vector to compute the new stiffness matrix.  It uses 
// the BFGS method used by Thewalt and later by Igarashi 
// (Igarashi 1993 p. 10)

#include "ETBfgs.h"

#include <elementAPI.h>

void* OPF_ETBfgs()
{
    // pointer to experimental tangent stiff that will be returned
    ExperimentalTangentStiff* theTangentStiff = 0;
    
    if (OPS_GetNumRemainingInputArgs() < 1) {
        opserr << "WARNING invalid number of arguments\n";
        opserr << "Want: expTangentStiff BFGS tag <-eps value>\n";
        return 0;
    }
    
    // tangent stiff tag
    int tag;
    int numdata = 1;
    if (OPS_GetIntInput(&numdata, &tag) != 0) {
        opserr << "WARNING invalid expTangentStiff BFGS tag\n";
        return 0;
    }
    
    // optional parameters
    double eps = 0.1;
    const char* type;
    while (OPS_GetNumRemainingInputArgs() > 0) {
        type = OPS_GetString();
        if (strcmp(type, "-eps") == 0) {
            numdata = 1;
            if (OPS_GetDoubleInput(&numdata, &eps) != 0) {
                opserr << "WARNING invalid epsilon value\n";
                opserr << "expTangentStiff BFGS " << tag << endln;
                return 0;
            }
        }
    }
    
    // parsing was successful, allocate the tangent stiff
    theTangentStiff = new ETBfgs(tag, eps);
    if (theTangentStiff == 0) {
        opserr << "WARNING could not create experimental tangent stiffness "
            << "of type ETBroyden\n";
        return 0;
    }
    
    return theTangentStiff;
}


ETBfgs::ETBfgs(int tag , double e)
    : ExperimentalTangentStiff(tag), eps(e), theStiff(0)
{
    // does nothing
}


ETBfgs::ETBfgs(const ETBfgs& ets)
    : ExperimentalTangentStiff(ets), theStiff(0)
{
    eps = ets.eps;
}


ETBfgs::~ETBfgs()
{
    // invoke the destructor on any objects created by the object
    // that the object still holds a pointer to
    if (theStiff != 0)
        delete theStiff;
}


Matrix& ETBfgs::updateTangentStiff(
    const Vector* incrDisp,
    const Vector* incrVel,
    const Vector* incrAccel,
    const Vector* incrForce,
    const Vector* time,
    const Matrix* kInit,
    const Matrix* kPrev)
{
    // using incremental disp and force
    int dimR = kPrev->noRows();
    int dimC = kPrev->noCols();
    int szD	 = incrDisp->Size();
    
    // FIX ME add size check
    theStiff = new Matrix(dimR, dimC);
    theStiff->Zero();
    
    double normD = incrDisp->Norm();
    double normF = incrForce->Norm();
    double eFactor = eps*normD*normF;
    double fD = 0.0;
    double fNDK1, fNDK2;
    Matrix tempDK1(dimR, dimC), tempDK2(dimR, dimC);
    
    // dot product between vectors
    int i;
    for (i=0; i<szD; i++) {
        fD += (*incrDisp)(i)*(*incrForce)(i);
    }
    
    if (eFactor >= fD) {
        theStiff->addMatrix(0.0, (*kPrev), 1.0);
    } else {
        this->calculateDeltaK(fD, incrDisp, incrForce, kInit, &tempDK1);
        this->calculateDeltaK(fD, incrDisp, incrForce, kPrev, &tempDK2);
        
        fNDK1 = this->fNorm(&tempDK1);
        fNDK2 = this->fNorm(&tempDK2);
        
        if (fNDK1 <= fNDK2) {
            theStiff->addMatrix(0.0, (*kInit), 1.0);
            theStiff->addMatrix(1.0, tempDK1, 1.0);
        } else {
            theStiff->addMatrix(0.0, (*kPrev), 1.0);
            theStiff->addMatrix(1.0, tempDK2, 1.0);
        }
    }
    
    return *theStiff;
}


ExperimentalTangentStiff* ETBfgs::getCopy()
{
    return new ETBfgs(*this);
}


void ETBfgs::Print(OPS_Stream &s, int flag)
{
    s << "Experimental Tangent: " << this->getTag();
    s << "  type: ETBfgs\n";
    s << "  eps: " << eps  << endln;
}


Response* ETBfgs::setResponse(const char **argv,
    int argc, OPS_Stream &output)
{
    Response *theResponse = 0;
    
    output.tag("ExpTangentStiffOutput");
    output.attr("tangStifType",this->getClassType());
    output.attr("tangStifTag",this->getTag());
    
    // stiffness
    if (strcmp(argv[0],"stif") == 0 ||
        strcmp(argv[0],"stiff") == 0 ||
        strcmp(argv[0],"stiffness") == 0)
    {
        output.tag("ResponseType","tangStif");
        theResponse = new ExpTangentStiffResponse(this, 1, Matrix(1,1));
    }
    
    return theResponse;
}


int ETBfgs::getResponse(int responseID,
    Information &info)
{
    switch (responseID)  {
    case 1:  // stiffness
        if (theStiff != 0)
            return info.setMatrix(*theStiff);
        
    default:
        return OF_ReturnType_failed;
    }
}


int ETBfgs::calculateDeltaK(double fD,
    const Vector* incrDisp,
    const Vector* incrForce,
    const Matrix* k,
    Matrix* dK)
{
    int numDOF = incrDisp->Size();
    double kFactor =  0.0;
    double invFD = 1.0/fD;
    Vector tempKD(numDOF);
    Matrix tempFD(numDOF, numDOF);
    Matrix tempDF(numDOF, numDOF);
    
    tempKD.Zero();
    tempKD.addMatrixVector(0.0, (*k), (*incrDisp), 1.0);
    dK->Zero();
    
    // dot product between vectors
    int i, j;
    for (i=0; i<numDOF; i++) {
        kFactor += (*incrDisp)(i) * tempKD(i);
    }
    kFactor = (1.0 + invFD*kFactor) * invFD;
    
    for (i=0; i<numDOF; i++) {
        for (j=0; j<numDOF; j++) {
            (*dK)(i,j) = kFactor * (*incrForce)(i) * (*incrForce)(j);
            tempFD(i,j) = invFD * (*incrForce)(i) * (*incrDisp)(j);
            tempDF(i,j) = invFD * (*incrDisp)(i) * (*incrForce)(j);
        }
    }
    
    dK->addMatrixProduct(1.0, tempFD, (*k), -1.0);
    dK->addMatrixProduct(1.0, (*k), tempDF, -1.0);
    
    return OF_ReturnType_completed;
}


double ETBfgs::fNorm(const Matrix* dK)
{
    int numDOF = dK->noRows();
    int i, j;
    double fN = 0.0;
    
    for (i=0; i<numDOF; i++) {
        for (j=0; j<numDOF; j++) {
            fN += abs((*dK)(i,j) * (*dK)(i,j));
        }
    }
    fN = sqrt(fN);
    
    return fN;
}
