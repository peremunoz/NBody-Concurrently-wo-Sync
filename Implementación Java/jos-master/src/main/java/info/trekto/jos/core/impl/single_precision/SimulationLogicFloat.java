/*
Práctica 2.
Código fuente: SimulationLogicFloat.java
Grau Informàtica
Pere Muñoz Figuerol
*/

package info.trekto.jos.core.impl.single_precision;

import com.aparapi.Kernel;
import info.trekto.jos.core.SimulationLogic;
import info.trekto.jos.core.impl.SimulationProperties;
import info.trekto.jos.core.model.SimulationObject;
import info.trekto.jos.core.model.impl.TripleNumber;
import info.trekto.jos.core.numbers.New;
import info.trekto.jos.core.numbers.Number;

import java.util.*;

import static info.trekto.jos.core.numbers.NumberFactoryProxy.ZERO;

public class SimulationLogicFloat extends Kernel implements SimulationLogic {
    private static final float TWO = 2.0f;
    private static final float RATIO_FOUR_THREE = 4 / 3.0f;
    private static final float GRAVITY = 0.000000000066743f; // 6.6743×10^−11 N⋅m2/kg2
    private static final float PI = (float)Math.PI;

    public final float[] positionX;
    public final float[] positionY;
    public final float[] velocityX;
    public final float[] velocityY;
    public final float[] accelerationX;
    public final float[] accelerationY;
    public final float[] mass;
    public final float[] radius;
    public final String[] id;
    public final int[] color;
    public final boolean[] deleted;

    public final float[] readOnlyPositionX;
    public final float[] readOnlyPositionY;
    public final float[] readOnlyVelocityX;
    public final float[] readOnlyVelocityY;
    public final float[] readOnlyAccelerationX;
    public final float[] readOnlyAccelerationY;
    public final float[] readOnlyMass;
    public final float[] readOnlyRadius;
    public final int[] readOnlyColor;
    public final boolean[] readOnlyDeleted;

    private final float secondsPerIteration;
    private final int screenWidth;
    private final int screenHeight;
    private final boolean mergeOnCollision;
    private final float coefficientOfRestitution;

    public SimulationLogicFloat(int numberOfObjects, float secondsPerIteration, int screenWidth, int screenHeight, boolean mergeOnCollision,
                                float coefficientOfRestitution) {
        int n = numberOfObjects;
        this.secondsPerIteration = secondsPerIteration;

        positionX = new float[n];
        positionY = new float[n];
        velocityX = new float[n];
        velocityY = new float[n];
        accelerationX = new float[n];
        accelerationY = new float[n];
        mass = new float[n];
        radius = new float[n];
        id = new String[n];
        color = new int[n];
        deleted = new boolean[n];

        readOnlyPositionX = new float[n];
        readOnlyPositionY = new float[n];
        readOnlyVelocityX = new float[n];
        readOnlyVelocityY = new float[n];
        readOnlyAccelerationX = new float[n];
        readOnlyAccelerationY = new float[n];
        readOnlyMass = new float[n];
        readOnlyRadius = new float[n];
        readOnlyColor = new int[n];
        readOnlyDeleted = new boolean[n];
        
        this.screenWidth = screenWidth;
        this.screenHeight = screenHeight;
        this.mergeOnCollision = mergeOnCollision;
        this.coefficientOfRestitution = coefficientOfRestitution;
    }

    @Override
    public void run() {
        calculateNewValues(getGlobalId());
    }

    /**
     * !!! DO NOT CHANGE THIS METHOD, run() method and methods called from them if you don't have experience with Aparapi library!!!
     * This code is translated to OpenCL and executed on the GPU.
     * You cannot use even simple 'break' here - it is not supported by Aparapi.
     */
    public void calculateNewValues(int i) {
        if (!deleted[i]) {
            /* Speed is scalar, velocity is vector. Velocity = speed + direction. */

            /* Time T passed */

            /* Calculate acceleration */
            /* For the time T, forces accelerated the objects (changed their velocities).
             * Forces are calculated having the positions of the objects at the beginning of the period,
             * and these forces are applied for time T. */
            float newAccelerationX = 0;
            float newAccelerationY = 0;
            for (int j = 0; j < readOnlyPositionX.length; j++) {
                if (i != j && !readOnlyDeleted[j]) {
                    /* Calculate force */
                    float distance = calculateDistance(positionX[i], positionY[i], readOnlyPositionX[j], readOnlyPositionY[j]);
                    float force = calculateForce(mass[i], readOnlyMass[j], distance);
                    //       Fx = F*x/r;
                    float forceX = force * (readOnlyPositionX[j] - positionX[i]) / distance;
                    float forceY = force * (readOnlyPositionY[j] - positionY[i]) / distance;

                    /* Add to current acceleration */
                    // ax = Fx / m
                    newAccelerationX = newAccelerationX + forceX / mass[i];
                    newAccelerationY = newAccelerationY + forceY / mass[i];
                }
            }

            /* Move objects */
            /* For the time T, velocity moved the objects (changed their positions).
             * New objects positions are calculated having the velocity at the beginning of the period,
             * and these velocities are applied for time T. */
            positionX[i] = positionX[i] + velocityX[i] * secondsPerIteration;
            positionY[i] = positionY[i] + velocityY[i] * secondsPerIteration;

            /* Change velocity */
            /* For the time T, accelerations changed the velocities.
             * Velocities are calculated having the accelerations of the objects at the beginning of the period,
             * and these accelerations are applied for time T. */
            velocityX[i] = velocityX[i] + accelerationX[i] * secondsPerIteration;
            velocityY[i] = velocityY[i] + accelerationY[i] * secondsPerIteration;
            
            /* Change the acceleration */
            accelerationX[i] = newAccelerationX;
            accelerationY[i] = newAccelerationY;
            
            /* Bounce from screen borders */
            if (screenWidth != 0 && screenHeight != 0) {
                bounceFromScreenBorders(i);
            }
        }
    }


    public void calculateAllNewValues(int numberOfThreads) {
        // Calculate variables for concurrent execution
        int numberOfObjects = positionX.length;
        int numberOfObjectsPerThread = numberOfObjects / numberOfThreads;
        int numberOfObjectsLeft = numberOfObjects % numberOfThreads;

        List<Thread> threads = new ArrayList<>();
        for (int i = 0; i < numberOfThreads; i++) {
            // Determine the number of objects for this thread
            int start = i * numberOfObjectsPerThread;
            int end = start + numberOfObjectsPerThread;
            if (i == numberOfThreads - 1) {
                end += numberOfObjectsLeft;
            }
            final int finalStart = start;
            final int finalEnd = end;
            Thread thread = new Thread(() -> {
                for (int j = finalStart; j < finalEnd; j++) {
                    calculateNewValues(j);
                }
            });
            threads.add(thread);
            thread.start();
        }
        // Wait for all threads to finish before next iteration
        for (Thread thread : threads) {
            try {
                thread.join();
            } catch (InterruptedException e) {
                e.printStackTrace();
                SimulationFloat.cancelThreads(threads);
            }
        }
    }

    private void bounceFromScreenBorders(int i) {
        if (positionX[i] + radius[i] >= screenWidth / 2.0 || positionX[i] - radius[i] <= -screenWidth / 2.0) {
            velocityX[i] = -velocityX[i];
        }

        if (positionY[i] + radius[i] >= screenHeight / 2.0 || positionY[i] - radius[i] <= -screenHeight / 2.0) {
            velocityY[i] = -velocityY[i];
        }
    }

    public void processCollisionsThread(int numberOfThreads) {
        // Calculate variables for concurrent execution
        int numberOfObjects = positionX.length;
        int numberOfObjectsPerThread = numberOfObjects / numberOfThreads;
        int numberOfObjectsLeft = numberOfObjects % numberOfThreads;

        List<Thread> threads = new ArrayList<>();
        for (int i = 0; i < numberOfThreads; i++) {
            // Determine the number of objects for this thread
            int start = i * numberOfObjectsPerThread;
            int end = start + numberOfObjectsPerThread;
            if (i == numberOfThreads - 1) {
                end += numberOfObjectsLeft;
            }
            final int finalStart = start;
            final int finalEnd = end;
            Thread thread = new Thread(() -> {
                processCollisions(finalStart, finalEnd);
            });
            threads.add(thread);
            thread.start();
        }
        // Wait for all threads to finish before next iteration
        for (Thread thread : threads) {
            try {
                thread.join();
            } catch (InterruptedException e) {
                e.printStackTrace();
                SimulationFloat.cancelThreads(threads);
            }
        }
    }

    public void processCollisions(int start, int end) {
        Set<Map.Entry<Integer, Integer>> processedElasticCollision = null;
        if (!mergeOnCollision) {
            processedElasticCollision = new HashSet<>();
        }
        for (int i = start; i < end; i++) {
            if (mergeOnCollision && deleted[i]) {
                continue;
            }
            for (int j = 0; j < positionX.length; j++) {
                if (i == j) {
                    continue;
                }

                if (mergeOnCollision) {
                if (deleted[j]) {
                    continue;
                }
                } else if (processedElasticCollision.contains(new AbstractMap.SimpleEntry<>(i, j))) {
                    continue;
                }

                float distance = calculateDistance(positionX[i], positionY[i], positionX[j], positionY[j]);
                        if (distance < radius[i] + radius[j]) {    // if collide
                    if (!mergeOnCollision) {
                        processTwoDimensionalCollision(i, j, coefficientOfRestitution);
                        processedElasticCollision.add(new AbstractMap.SimpleEntry<>(i, j));
                        processedElasticCollision.add(new AbstractMap.SimpleEntry<>(j, i));
                    } else {
                            /* Objects merging */
                            int bigger;
                            int smaller;
                            if (mass[i] < mass[j]) {
                                bigger = j;
                                smaller = i;
                            } else {
                                bigger = i;
                                smaller = j;
                            }

                            deleted[smaller] = true;

                            /* Velocity */
                            changeVelocityOnMerging(smaller, bigger);

                            /* Position */
                            changePositionOnMerging(smaller, bigger);

                            /* Color */
                            color[bigger] = calculateColor(smaller, bigger);

                            /* Volume (radius) */
                            radius[bigger] = calculateRadiusBasedOnNewVolumeAndDensity(smaller, bigger);

                            /* Mass */
                            mass[bigger] = mass[bigger] + mass[smaller];

                            if (i == smaller) {
                                /* If the current object is deleted stop processing it further. */
                                break;
                            }
                        }
                    }
                }
            }
        }

    /**
     * Because processCollisions() method does not run on GPU
     * we can remove this method and replace color encode/decode with java.awt.Color
     */
    private int calculateColor(int smaller, int bigger) {
        float bigMass = mass[bigger];
        float smallMass = mass[smaller];

        /* Decode color */
        int biggerRed = (color[bigger] >> 16) & 0xFF;
        int biggerGreen = (color[bigger] >> 8) & 0xFF;
        int biggerBlue = color[bigger] & 0xFF;

        int smallerRed = (color[smaller] >> 16) & 0xFF;
        int smallerGreen = (color[smaller] >> 8) & 0xFF;
        int smallerBlue = color[smaller] & 0xFF;

        /* Calculate new value */
        int r = Math.round((biggerRed * bigMass + smallerRed * smallMass) / (bigMass + smallMass));
        int g = Math.round((biggerGreen * bigMass + smallerGreen * smallMass) / (bigMass + smallMass));
        int b = Math.round((biggerBlue * bigMass + smallerBlue * smallMass) / (bigMass + smallMass));

        /* Encode color */
        int rgb = r;
        rgb = (rgb << 8) + g;
        rgb = (rgb << 8) + b;

        return rgb;
    }

    /**
     * We calculate for sphere, not for circle, so in 2D volume may not look real.
     */
    public float calculateRadiusBasedOnNewVolumeAndDensity(int smaller, int bigger) {
        // density = mass / volume
        // calculate volume of smaller and add it to volume of bigger
        // calculate new radius of bigger based on new volume
        float smallVolume = calculateVolumeFromRadius(radius[smaller]);
        float smallDensity = mass[smaller] / smallVolume;
        float bigVolume = calculateVolumeFromRadius(radius[bigger]);
        float bigDensity = mass[bigger] / bigVolume;
        float newMass = mass[bigger] + mass[smaller];

        /* Volume and density are two sides of one coin. We should decide what we want one of them to be,
         * and calculate the other. Here we want the new object to have an average density of the two collided. */
        float newDensity = (smallDensity * mass[smaller] + bigDensity * mass[bigger]) / newMass;
        float newVolume = newMass / newDensity;

        return calculateRadiusFromVolume(newVolume);
    }

    private void changePositionOnMerging(int smaller, int bigger) {
        float distanceX = positionX[bigger] - positionX[smaller];
        float distanceY = positionY[bigger] - positionY[smaller];

        float massRatio = mass[smaller] / mass[bigger];

        positionX[bigger] = positionX[bigger] - distanceX * massRatio / TWO;
        positionY[bigger] = positionY[bigger] - distanceY * massRatio / TWO;
    }

    private void changeVelocityOnMerging(int smaller, int bigger) {
        /* We want to get already updated velocity for the current one (bigger), thus we use velocityX and not readOnlyVelocityX */
        float totalImpulseX = velocityX[smaller] * mass[smaller] + velocityX[bigger] * mass[bigger];
        float totalImpulseY = velocityY[smaller] * mass[smaller] + velocityY[bigger] * mass[bigger];
        float totalMass = mass[bigger] + mass[smaller];

        velocityX[bigger] = totalImpulseX / totalMass;
        velocityY[bigger] = totalImpulseY / totalMass;
    }

    public static float calculateDistance(float object1X, float object1Y, float object2X, float object2Y) {
        float x = object2X - object1X;
        float y = object2Y - object1Y;
        return (float)Math.sqrt(x * x + y * y);
    }

    public static float calculateForce(final float object1Mass, final float object2Mass, final float distance) {
        return GRAVITY * object1Mass * object2Mass / (distance * distance);
    }

    public static float calculateVolumeFromRadius(float radius) {
        // V = 4/3 * pi * r^3
        if (radius == 0) {
            return Float.MIN_VALUE;
        }
        return RATIO_FOUR_THREE * PI * (float)Math.pow(radius, 3);
    }

    public static float calculateRadiusFromVolume(float volume) {
        // V = 4/3 * pi * r^3
        if (volume == 0) {
            return Float.MIN_VALUE;
        }
        return (float)Math.cbrt(volume / (RATIO_FOUR_THREE * PI));
    }

    private void processTwoDimensionalCollision(int o1, int o2, float cor) {
        float v1x = velocityX[o1];
        float v1y = velocityY[o1];
        float v2x = velocityX[o2];
        float v2y = velocityY[o2];
        
        float o1x = positionX[o1];
        float o1y = positionY[o1];
        float o2x = positionX[o2];
        float o2y = positionY[o2];
        
        float o1m = mass[o1];
        float o2m = mass[o2];
        
        // v'1y = v1y - 2*m2/(m1+m2) * dotProduct(o1, o2) / dotProduct(o1y, o1x, o2y, o2x) * (o1y-o2y)
        // v'2x = v2x - 2*m2/(m1+m2) * dotProduct(o2, o1) / dotProduct(o2x, o2y, o1x, o1y) * (o2x-o1x)
        // v'2y = v2y - 2*m2/(m1+m2) * dotProduct(o2, o1) / dotProduct(o2y, o2x, o1y, o1x) * (o2y-o1y)
        // v'1x = v1x - 2*m2/(m1+m2) * dotProduct(o1, o2) / dotProduct(o1x, o1y, o2x, o2y) * (o1x-o2x)
        velocityX[o1] = calculateVelocity(v1x, v1y, v2x, v2y, o1x, o1y, o2x, o2y, o1m, o2m, cor);
        velocityY[o1] = calculateVelocity(v1y, v1x, v2y, v2x, o1y, o1x, o2y, o2x, o1m, o2m, cor);
        velocityX[o2] = calculateVelocity(v2x, v2y, v1x, v1y, o2x, o2y, o1x, o1y, o2m, o1m, cor);
        velocityY[o2] = calculateVelocity(v2y, v2x, v1y, v1x, o2y, o2x, o1y, o1x, o2m, o1m, cor);
    }

    private float calculateVelocity(float v1x, float v1y, float v2x, float v2y,
                                    float o1x, float o1y, float o2x, float o2y, float o1m, float o2m, float cor) {
        // v'1x = v1x - 2*o2m/(o1m+o2m) * dotProduct(o1, o2) / dotProduct(o1x, o1y, o2x, o2y) * (o1x-o2x)
        return v1x - (cor * o2m + o2m) / (o1m + o2m)
                * dotProduct2D(v1x, v1y, v2x, v2y, o1x, o1y, o2x, o2y)
                / dotProduct2D(o1x, o1y, o2x, o2y, o1x, o1y, o2x, o2y)
                * (o1x - o2x);
    }

    private float dotProduct2D(float ax, float ay, float bx, float by, float cx, float cy, float dx, float dy) {
        // <a - b, c - d> = (ax - bx) * (cx - dx) + (ay - by) * (cy - dy)
        return (ax - bx) * (cx - dx) + (ay - by) * (cy - dy);
    }

    /**
     * For testing only.
     */
    @Override
    public void processTwoDimensionalCollision(SimulationObject o1, SimulationObject o2, Number cor) {
        velocityX[0] = o1.getVelocity().getX().floatValue();
        velocityY[0] = o1.getVelocity().getY().floatValue();
        velocityX[1] = o2.getVelocity().getX().floatValue();
        velocityY[1] = o2.getVelocity().getY().floatValue();
        
        positionX[0] = o1.getX().floatValue();
        positionY[0] = o1.getY().floatValue();
        positionX[1] = o2.getX().floatValue();
        positionY[1] = o2.getY().floatValue();
        
        mass[0] = o1.getMass().floatValue();
        mass[1] = o2.getMass().floatValue();
        
        processTwoDimensionalCollision(0, 1, cor.floatValue());
        
        o1.setVelocity(new TripleNumber(New.num(velocityX[0]), New.num(velocityY[0]), ZERO));
        o2.setVelocity(new TripleNumber(New.num(velocityX[1]), New.num(velocityY[1]), ZERO));
    }
}
