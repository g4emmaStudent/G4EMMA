//
// ********************************************************************
// * License and Disclaimer                                           *
// *                                                                  *
// * The  Geant4 software  is  copyright of the Copyright Holders  of *
// * the Geant4 Collaboration.  It is provided  under  the terms  and *
// * conditions of the Geant4 Software License,  included in the file *
// * LICENSE and available at  http://cern.ch/geant4/license .  These *
// * include a list of copyright holders.                             *
// *                                                                  *
// * Neither the authors of this software system, nor their employing *
// * institutes,nor the agencies providing financial support for this *
// * work  make  any representation or  warranty, express or implied, *
// * regarding  this  software system or assume any liability for its *
// * use.  Please see the license in the file  LICENSE  and URL above *
// * for the full disclaimer and the limitation of liability.         *
// *                                                                  *
// * This  code  implementation is the result of  the  scientific and *
// * technical work of the GEANT4 collaboration.                      *
// * By using,  copying,  modifying or  distributing the software (or *
// * any work based  on the software)  you  agree  to acknowledge its *
// * use  in  resulting  scientific  publications,  and indicate your *
// * acceptance of all terms of the Geant4 Software license.          *
// ********************************************************************
//
/// \file field/field04/src/F04GlobalField.cc
/// \brief Implementation of the F04GlobalField class
//

///  F04GlobalField - handles the global ElectroMagnetic field
///
///  There is a single G04GlobalField object.
///
///  The field from each individual beamline element (quad, ED, etc.) is given by a
///  ElementField object. Any number of overlapping ElementField
///  objects can be added to the global field. Any element that
///  with an EM field must add the appropriate
///  ElementField to the global GlobalField object.
//
#include <time.h>

#include "Randomize.hh"
#include "G4TransportationManager.hh"

#include "G4ExplicitEuler.hh"
#include "G4ImplicitEuler.hh"
#include "G4SimpleRunge.hh"
#include "G4SimpleHeum.hh"
#include "G4ClassicalRK4.hh"
#include "G4CashKarpRKF45.hh"
#include "G4SystemOfUnits.hh"

#include "EMMAGlobalField.hh"
#include "EMMAElementField.hh"
#include "EMFieldDebugger.hh"

EMMAGlobalField* EMMAGlobalField::fObject = 0;

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

EMMAGlobalField::EMMAGlobalField() : G4ElectroMagneticField(),
                             fMinStep(0.01*mm), fDeltaChord(3.0*mm),
                             fDeltaOneStep(0.01*mm), fDeltaIntersection(0.1*mm),
                             fEpsMin(2.5e-7*mm), fEpsMax(0.05*mm),
                             fEquation(0), fFieldManager(0),
                             fFieldPropagator(0), fStepper(0), fChordFinder(0)
//EMMAGlobalField::EMMAGlobalField() : G4MagneticField(),
//                           fMinStep(0.01*mm), fDeltaChord(3.0*mm),
//                           fDeltaOneStep(0.01*mm), fDeltaIntersection(0.1*mm),
//                           fEpsMin(2.5e-7*mm), fEpsMax(0.05*mm),
//                           fEquation(0), fFieldManager(0),
//                           fFieldPropagator(0), fStepper(0), fChordFinder(0)
{
 // fFieldMessenger = new EMMAFieldMessenger(this);

  fFields = new FieldList();

  fStepperType = 4 ;       // ClassicalRK4 is default stepper

  //  set object

  fObject = this;

  UpdateField();
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

EMMAGlobalField::~EMMAGlobalField()
{
  Clear();

  //delete fFieldMessenger;

  if (fEquation)        delete fEquation;
  if (fFieldManager)    delete fFieldManager;
  if (fFieldPropagator) delete fFieldPropagator;
  if (fStepper)         delete fStepper;
  if (fChordFinder)     delete fChordFinder;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void EMMAGlobalField::UpdateField()
{
  fFirst = true;

  fNfp = 0;
  fFp = 0;

  Clear();

//  Construct equ. of motion of particles through B fields
//  fEquation = new G4Mag_EqRhs(this);
//    Construct equ. of motion of particles through e.m. fields
//  fEquation = new G4EqMagElectricField(this);
//  Construct equ. of motion of particles including spin through B fields
//  fEquation = new G4Mag_SpinEqRhs(this);
//  Construct equ. of motion of particles including spin through e.m. fields
  fEquation = new G4EqEMFieldWithSpin(this);

  //  Get transportation, field, and propagator managers
  G4TransportationManager* transportManager =
         G4TransportationManager::GetTransportationManager();

  fFieldManager = GetGlobalFieldManager();

  fFieldPropagator = transportManager->GetPropagatorInField();

  //  Need to SetFieldChangesEnergy to account for a time varying electric
  //  field (r.f. fields)
  fFieldManager->SetFieldChangesEnergy(true);

  //  Set the field
  fFieldManager->SetDetectorField(this);

  //  Choose a stepper for integration of the equation of motion
  SetStepper();

  //  Create a cord finder providing the (global field, min step length,
  //  a pointer to the stepper)
  fChordFinder = new G4ChordFinder((G4MagneticField*)this,fMinStep,fStepper);

  // Set accuracy parameters
  fChordFinder->SetDeltaChord(fDeltaChord);

  fFieldManager->SetAccuraciesWithDeltaOneStep(fDeltaOneStep);

  fFieldManager->SetDeltaIntersection(fDeltaIntersection);

  fFieldPropagator->SetMinimumEpsilonStep(fEpsMin);
  fFieldPropagator->SetMaximumEpsilonStep(fEpsMax);

  G4cout << "Accuracy Parameters:" <<
            " MinStep=" << fMinStep <<
            " DeltaChord=" << fDeltaChord <<
            " DeltaOneStep=" << fDeltaOneStep << G4endl;
  G4cout << "                    " <<
            " DeltaIntersection=" << fDeltaIntersection <<
            " EpsMin=" << fEpsMin <<
            " EpsMax=" << fEpsMax <<  G4endl;

  fFieldManager->SetChordFinder(fChordFinder);

}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

EMMAGlobalField* EMMAGlobalField::GetObject()
{
  if (!fObject) new EMMAGlobalField();
  return fObject;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void EMMAGlobalField::SetStepper()
{
  if(fStepper) delete fStepper;

  switch ( fStepperType )
  {
    case 0:
//      fStepper = new G4ExplicitEuler( fEquation, 8 ); // no spin tracking
      fStepper = new G4ExplicitEuler( fEquation, 12 ); // with spin tracking
      G4cout << "G4ExplicitEuler is called" << G4endl;
      break;
    case 1:
//      fStepper = new G4ImplicitEuler( fEquation, 8 ); // no spin tracking
      fStepper = new G4ImplicitEuler( fEquation, 12 ); // with spin tracking
      G4cout << "G4ImplicitEuler is called" << G4endl;
      break;
    case 2:
//      fStepper = new G4SimpleRunge( fEquation, 8 ); // no spin tracking
      fStepper = new G4SimpleRunge( fEquation, 12 ); // with spin tracking
      G4cout << "G4SimpleRunge is called" << G4endl;
      break;
    case 3:
//      fStepper = new G4SimpleHeum( fEquation, 8 ); // no spin tracking
      fStepper = new G4SimpleHeum( fEquation, 12 ); // with spin tracking
      G4cout << "G4SimpleHeum is called" << G4endl;
      break;
    case 4:
//      fStepper = new G4ClassicalRK4( fEquation, 8 ); // no spin tracking
      fStepper = new G4ClassicalRK4( fEquation, 12 ); // with spin tracking
      G4cout << "G4ClassicalRK4 (default) is called" << G4endl;
      break;
    case 5:
//      fStepper = new G4CashKarpRKF45( fEquation, 8 ); // no spin tracking
      fStepper = new G4CashKarpRKF45( fEquation, 12 ); // with spin tracking
      G4cout << "G4CashKarpRKF45 is called" << G4endl;
      break;
    default: fStepper = 0;
  }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

G4FieldManager* EMMAGlobalField::GetGlobalFieldManager()
{
  return G4TransportationManager::GetTransportationManager()
                                ->GetFieldManager();
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void EMMAGlobalField::GetFieldValue(const G4double* point, G4double* field) const
{
  // NOTE: this routine dominates the CPU time for tracking.
  // Using the simple array fFp[] instead of fields[]
  // directly sped it up

  field[0] = field[1] = field[2] = field[3] = field[4] = field[5] = 0.0;

  // protect against Geant4 bug that calls us with point[] NaN.
  if(point[0] != point[0]) return;

  // (can't use fNfp or fFp, as they may change)
  if (fFirst) ((EMMAGlobalField*)this)->SetupArray();   // (cast away const)
  for (int i=0; i<fNfp; ++i) {
      const EMMAElementField* p = fFp[i];
      if (p->IsInBoundingBox(point)) {
         p->AddFieldValue(point,field);
      }
  }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......


void EMMAGlobalField::Clear()
{
  if (fFields) {
     if (fFields->size()>0) {
        FieldList::iterator i;
        for (i=fFields->begin(); i!=fFields->end(); ++i) delete *i;
        fFields->clear();
     }
  }
  if (fFp) delete[] fFp;

  fFirst = true;
  fNfp = 0;
  fFp = NULL;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void EMMAGlobalField::SetupArray()
{
  fFirst = false;
  fNfp = fFields->size();
  fFp = new const EMMAElementField* [fNfp+1]; // add 1 so it's never 0
  for (int i=0; i<fNfp; ++i) fFp[i] = (*fFields)[i];
}
