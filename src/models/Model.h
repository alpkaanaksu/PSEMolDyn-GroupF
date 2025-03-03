//
// Created by Alp Kaan Aksu on 01.11.23.
//

#pragma once

#include <utility>

#include "functional"
#include "Particle.h"
#include "utils/ArrayUtils.h"
#include <array>

/**
 * Model class
 * Represents a model for a simulation
 */
class Model {
private:
    /**
     * Formula that is used to calculate the forces on particles
     */
    std::function<void(Particle &, Particle &)> force;

    /**
     * Formula that is used to calculate the positions of particles
     */
    std::function<void(Particle &)> position;

    /**
     * Formula that is used to calculate the velocity of particles
     */
    std::function<void(Particle &)> velocity;

public:
    Model(std::function<void(Particle &, Particle &)> force, std::function<void(Particle &)> position,
          std::function<void(Particle &)> velocity) : force(std::move(force)), position(std::move(position)),
                                                      velocity(std::move(velocity)) {};

    Model() : force([](Particle &p1, Particle &p2) {}), position([](Particle &p) {}), velocity([](Particle &p) {}) {};


    /**
     * @brief Returns a function that takes a particle as a parameter and sets the force values of that particle along all axis to 0
     *
     * @return The lambda function for resetting the force
     */

    static std::function<void(Particle &)> resetForceFunction() {
        return [](Particle &p) {
            p.updateF(std::array < double, 3 > {0., 0., 0.});
        };
    };


    /**
     * @brief Returns a force vector, which if applied to a particle, would result in a force that is
     *        equal to the gravitational force working on the particle
     *
     * @param m The mass of the particle, which the force vector is calculated for
     * @param g The gravitational acceleration constant
     * @return The force vector representing the gravitational force working on the particle
     */
    static std::array<double, 3> verticalGravityForce(double m, double g) {
        return {0, m * g, 0};
    }

    /**
     * @brief Returns the function for force
     *
     * @return The function for force
     */
    std::function<void(Particle &, Particle &)> forceFunction() {
        return force;
    }

    /**
     * @brief Returns the function for position
     *
     * @return The lambda function for position
     */
    std::function<void(Particle &)> positionFunction() {
        return position;
    }

    /**
     * @brief Returns the function for velocity
     *
     * @return The lambda function for velocity
     */
    std::function<void(Particle &)> velocityFunction() {
        return velocity;
    }

    /**
     * @brief Return a basic model
     *
     * @param deltaT Time delta for the model
     * @return A Model object configured with the most basic formulas
     */
    static Model gravityModel(double deltaT) {
        auto force = [](Particle &p1, Particle &p2) {
            auto nextForce =
                    p1.getM() * p2.getM() / std::pow(p2.distanceTo(p1), 3) * p1.diffTo(p2);

            p1.setF(p1.getF() + nextForce);
        };

        auto position = [deltaT](Particle &p) {
            auto x =
                    p.getX() +
                    deltaT * p.getV() +
                    (deltaT * deltaT / (2 * p.getM())) * p.getF();

            p.setX(x);
        };

        auto velocity = [deltaT](Particle &p) {
            auto v =
                    p.getV() +
                    (deltaT / (2 * p.getM())) * (p.getOldF() + p.getF());

            p.setV(v);
        };

        return Model{force, position, velocity};
    };

    /**
     * @brief Return a model that utilizes Lennard Jones potential for force calculation between particles
     *
     * @param deltaT Time delta for the model
     * @param epsilon represents the depth of the potential well, which reflects the strength of the interaction between the particles
     * @param sigma distance at which the inter-particle potential is zero.
     * @return A Model object configured with the basic formulas for velocity, position calculation and Lennard Jones potential formula for force calculation
     */
    static Model lennardJonesModel(double deltaT) {
        auto ljForce = [](Particle &p1, Particle &p2) {
            // Lorentz-Berthelot mixing rules
            auto epsilon = std::sqrt(p1.getEpsilon() * p2.getEpsilon());
            auto sigma = (p1.getSigma() + p2.getSigma()) / 2;

            auto distance = p2.distanceTo(p1);
            //todo remove later
            //distance = std::max(distance, 0.1);

            auto distance6 = std::pow(distance, 6);
            auto sigma6 = std::pow(sigma, 6);

            auto nextForce = (-24 * epsilon / std::pow(distance, 2))
                             * ((sigma6 / distance6) - 2 * (std::pow(sigma6, 2) / std::pow(distance6, 2)))
                             * p2.diffTo(p1);

            if(!p1.isFixed()){
                 p1.setF(p1.getF() + nextForce);
            }
            if(!p2.isFixed()){
                 p2.setF(p2.getF() - nextForce);
            }


        };

        auto position = [deltaT](Particle &p) {
            auto x =
                    p.getX() +
                    deltaT * p.getV() +
                    (deltaT * deltaT / (2 * p.getM())) * p.getF();

            if(!p.isFixed()){
                p.setX(x);
            }
        };

        auto velocity = [deltaT](Particle &p) {
            auto v =
                    p.getV() +
                    (deltaT / (2 * p.getM())) * (p.getOldF() + p.getF());

            if(!p.isFixed()){
                p.setV(v);

            }
        };

        return Model{ljForce, position, velocity};
    }


    /**
     * @brief Also generates a smoothed Lennard-Jones potential model for particle interactions.
     *
     * This L-J model functions same as the previous one, with the only difference potential function being truncated at a
     * distance of 2^(1/6) * sigma, ensuring only the repulsive part is active to prevent self-penetration of the membrane.
     *
     * @param deltaT The time step for the simulation.
     * @param cutoffRadius The cutoff radius for Lennard-Jones potential calculations.
     * @param smoothedRadius The radius at which the potential is smoothly transitioned.
     * @return A Model object representing the smoothed Lennard-Jones potential model.
     *
     * The smoothing effect is applied when the distance between particles is less than or equal to the specified smoothed radius.
     * For distances beyond the smoothed radius but within the cutoff radius, a modified Lennard-Jones potential is used.
     */

    static Model smoothedLennardJonesModel(double deltaT, double cutoffRadius, double smoothedRadius) {
        auto ljForce = [smoothedRadius, cutoffRadius](Particle &p1, Particle &p2) {
            auto epsilon = std::sqrt(p1.getEpsilon() * p2.getEpsilon());
            auto sigma = (p1.getSigma() + p2.getSigma()) / 2;

            auto distance = p2.distanceTo(p1);
            auto distance6 = std::pow(distance, 6);
            auto distance14 = std::pow(distance, 14);
            auto sigma6 = std::pow(sigma, 6);

            if (distance <= smoothedRadius) {
                // force from non-smoothed lennard jones
                auto nextForce = (-24 * epsilon / std::pow(distance, 2))
                                 * ((sigma6 / distance6) - 2 * (std::pow(sigma6, 2) / std::pow(distance6, 2)))
                                 * p2.diffTo(p1);
                p1.setF(p1.getF() + nextForce);
                p2.setF(p2.getF() - nextForce);
            } else if (distance < cutoffRadius) {
                auto term1 = ((-24 * sigma6 * epsilon) / (distance14 * std::pow((cutoffRadius - smoothedRadius), 3)))
                             *(cutoffRadius - distance) * (p2.getX() - p1.getX()) ;
                auto term2 = (std::pow(cutoffRadius, 2) * (2 * sigma6 - distance6)) +
                             (cutoffRadius * (3 * smoothedRadius - distance) * (distance6 - 2 * sigma6)) +
                              (distance * ((5 * smoothedRadius * sigma6) - (2 * smoothedRadius * distance6) -
                                           (3 * sigma6 * distance) + std::pow(distance, 7)));
                auto nextForce = term2 * term1;
                p1.setF(p1.getF() + nextForce);
                p2.setF(p2.getF() - nextForce);
            }

        };

        auto position = [deltaT](Particle &p) {
            auto x =
                    p.getX() +
                    deltaT * p.getV() +
                    (deltaT * deltaT / (2 * p.getM())) * p.getF();

            p.setX(x);
        };

        auto velocity = [deltaT](Particle &p) {
            auto v =
                    p.getV() +
                    (deltaT / (2 * p.getM())) * (p.getOldF() + p.getF());

            p.setV(v);
        };
        return Model{ljForce, position, velocity};
    }

    static Model membraneModel(double deltaT) {
        auto ljForce = [](Particle &p1, Particle &p2) {
            double cutoffDistance = std::pow(2, (1.0/6.0)) * p1.getSigma();
            // Skip force calculation if beyond cutoff distance, Lennard-Jones only as repulsive force

            if (p1.distanceTo(p2) <= cutoffDistance) {
                auto epsilon = std::sqrt(p1.getEpsilon() * p2.getEpsilon());
                auto sigma = (p1.getSigma() + p2.getSigma()) / 2;

                auto distance = p2.distanceTo(p1);
                auto distance6 = std::pow(distance, 6);
                auto sigma6 = std::pow(sigma, 6);

                auto nextForce = (-24 * epsilon / std::pow(distance,2))
                                 * ((sigma6/distance6) - 2*(std::pow(sigma6,2)/std::pow(distance6,2)))
                                 * p2.diffTo(p1);

                p1.setF(p1.getF() + nextForce);
                p2.setF(p2.getF() - nextForce);
            }
        };

        auto position = [deltaT](Particle &p) {
            auto x =
                    p.getX() +
                    deltaT * p.getV() +
                    (deltaT * deltaT / (2 * p.getM())) * p.getF();

            p.setX(x);
        };

        auto velocity = [deltaT](Particle &p) {
            auto v =
                    p.getV() +
                    (deltaT / (2 * p.getM())) * (p.getOldF() + p.getF());

            p.setV(v);
        };

        return Model{ljForce, position, velocity};
    }
};