/*
Práctica 2.
Código fuente: CollisionCheckFloat.java
Grau Informàtica
Pere Muñoz Figuerol
*/

package info.trekto.jos.core.impl.single_precision;

import com.aparapi.Kernel;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public class CollisionCheckFloat extends Kernel {
    public final boolean[] collisions;
    public final int n;

    public final float[] positionX;
    public final float[] positionY;
    public final float[] radius;
    public final boolean[] deleted;

    public CollisionCheckFloat(int n, float[] positionX, float[] positionY, float[] radius, boolean[] deleted) {
        this.n = n;
        collisions = new boolean[n];

        this.positionX = positionX;
        this.positionY = positionY;
        this.radius = radius;
        this.deleted = deleted;
    }

    public void prepare() {
        Arrays.fill(collisions, false);
    }

    public boolean collisionExists() {
        for (boolean collision : collisions) {
            if (collision) {
                return true;
            }
        }
        return false;
    }

    /**
     * !!! DO NOT CHANGE THIS METHOD and methods called from it if you don't have experience with Aparapi library!!!
     * This code is translated to OpenCL and executed on the GPU.
     * You cannot use even simple 'break' here - it is not supported by Aparapi.
     */
    @Override
    public void run() {
        int i = getGlobalId();
        if (!deleted[i]) {
            boolean collision = false;
            for (int j = 0; j < n; j++) {
                if (!collision && i != j && !deleted[j]) {
                    // distance between centres
                    float x = positionX[j] - positionX[i];
                    float y = positionY[j] - positionY[i];
                    float distance = (float)Math.sqrt(x * x + y * y);

                    if (distance < radius[i] + radius[j]) {
                        collision = true;
                        collisions[i] = true;
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
        for (int i = start; i < end; i++) {
            if (!deleted[i]) {
                boolean collision = false;
                for (int j = 0; j < n; j++) {
                    if (!collision && i != j && !deleted[j]) {
                        // distance between centres
                        double x = positionX[j] - positionX[i];
                        double y = positionY[j] - positionY[i];
                        double distance = Math.sqrt(x * x + y * y);

                        if (distance < radius[i] + radius[j]) {
                            collision = true;
                            collisions[i] = true;
                        }
                    }
                }
            }
        }
    }

}
