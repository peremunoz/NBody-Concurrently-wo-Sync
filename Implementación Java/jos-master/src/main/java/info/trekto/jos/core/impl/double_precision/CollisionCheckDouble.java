/*
Práctica 2.
Código fuente: CollisionCheckDouble.java
Grau Informàtica
48252062V - Pere Muñoz Figuerol
*/

package info.trekto.jos.core.impl.double_precision;

import com.aparapi.Kernel;
import info.trekto.jos.core.impl.single_precision.SimulationFloat;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public class CollisionCheckDouble extends Kernel {
    public final boolean[] collisionExists;
    public final int n;

    public final double[] positionX;
    public final double[] positionY;
    public final double[] radius;
    public final boolean[] deleted;

    public CollisionCheckDouble(int n, double[] positionX, double[] positionY, double[] radius, boolean[] deleted) {
        this.n = n;
        collisionExists = new boolean[1];

        this.positionX = positionX;
        this.positionY = positionY;
        this.radius = radius;
        this.deleted = deleted;
    }

    public void prepare() {
        collisionExists[0] = false;
    }

    public boolean collisionExists() {
        return collisionExists[0];
    }

    /**
     * !!! DO NOT CHANGE THIS METHOD and methods called from it if you don't have experience with Aparapi library!!!
     * This code is translated to OpenCL and executed on the GPU.
     * You cannot use even simple 'break' here - it is not supported by Aparapi.
     */
    @Override
    public void run() {
        if (collisionExists[0]) {
            return;
        }
        int i = getGlobalId();
        if (!deleted[i]) {
            for (int j = 0; j < n; j++) {
                if (i != j && !deleted[j]) {
                    // distance between centres
                    double x = positionX[j] - positionX[i];
                    double y = positionY[j] - positionY[i];
                    double distance = Math.sqrt(x * x + y * y);

                    if (distance < radius[i] + radius[j]) {
                        collisionExists[0] = true;
                        return;
                    }
                }
            }
        }
    }

    public void checkAllCollisionsThread(int numberOfThreads) {
        // Calculate variables for concurrent execution
        int numberOfObjects = positionX.length;
        int numberOfObjectsPerThread = numberOfObjects / numberOfThreads;
        int numberOfObjectsLeft = numberOfObjects % numberOfThreads;

        // Create threads
        List<Thread> threads = new ArrayList<>();
        for (int i = 0; i < numberOfThreads; i++) {
            // Calculate start and end index for this thread
            int start = i * numberOfObjectsPerThread;
            int end = start + numberOfObjectsPerThread;
            if (i == numberOfThreads - 1) {
                end += numberOfObjectsLeft;
            }
            final int finalStart = start;
            final int finalEnd = end;
            Thread thread = new Thread(() -> {
                checkAllCollisions(finalStart, finalEnd);
            });
            threads.add(thread);
            thread.start();
        }

        // Wait for all threads to finish
        for (Thread thread : threads) {
            try {
                thread.join();
            } catch (InterruptedException e) {
                e.printStackTrace();
                SimulationFloat.cancelThreads(threads);
            }
        }
    }

    public void checkAllCollisions(int start, int end) {
        if (collisionExists[0]) {
            return;
        }
        for (int i = start; i < end; i++) {
            if (!deleted[i]) {
                for (int j = 0; j < n; j++) {
                    if (i != j && !deleted[j]) {
                        // distance between centres
                        double x = positionX[j] - positionX[i];
                        double y = positionY[j] - positionY[i];
                        double distance = Math.sqrt(x * x + y * y);

                        if (distance < radius[i] + radius[j]) {
                            collisionExists[0] = true;
                            return;
                        }
                    }
                }
            }
        }
    }

}
