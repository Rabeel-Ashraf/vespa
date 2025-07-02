// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.vespa.model.content;

import com.yahoo.config.application.api.DeployLogger;
import com.yahoo.vespa.model.builder.xml.dom.ModelElement;
import com.yahoo.vespa.model.content.cluster.DomResourceLimitsBuilder;

import java.util.Optional;
import java.util.function.Consumer;

import static java.util.logging.Level.WARNING;

/**
 * Class tracking the feed block resource limits for a content cluster.
 *
 * This includes the limits used by the cluster controller and the content nodes (proton).
 *
 * @author Geir Storli
 */
public class ClusterResourceLimits {

    private final ResourceLimits clusterControllerLimits;
    private final ResourceLimits contentNodeLimits;

    private ClusterResourceLimits(Builder builder) {
        clusterControllerLimits = builder.ctrlBuilder.build();
        contentNodeLimits = builder.nodeBuilder.build();
    }

    public ResourceLimits getClusterControllerLimits() {
        return clusterControllerLimits;
    }

    public ResourceLimits getContentNodeLimits() {
        return contentNodeLimits;
    }

    public static class Builder {

        private final boolean hostedVespa;
        private final double resourceLimitDisk;
        private final double resourceLimitMemory;
        private final double resourceLimitLowWatermarkDifference;
        private final double resourceLimitLowAddressSpace;
        private final DeployLogger deployLogger;

        private ResourceLimits.Builder ctrlBuilder = new ResourceLimits.Builder();
        private ResourceLimits.Builder nodeBuilder = new ResourceLimits.Builder();

        public Builder(boolean hostedVespa,
                       double resourceLimitDisk,
                       double resourceLimitMemory,
                       double resourceLimitLowWatermarkDifference,
                       double resourceLimitLowAddressSpace,
                       DeployLogger deployLogger) {
            this.hostedVespa = hostedVespa;
            this.resourceLimitDisk = resourceLimitDisk;
            this.resourceLimitMemory = resourceLimitMemory;
            this.resourceLimitLowWatermarkDifference = resourceLimitLowWatermarkDifference;
            this.resourceLimitLowAddressSpace = resourceLimitLowAddressSpace;
            this.deployLogger = deployLogger;
            verifyLimits(resourceLimitDisk, resourceLimitMemory, resourceLimitLowWatermarkDifference, resourceLimitLowAddressSpace);
        }

        public ClusterResourceLimits build(ModelElement clusterElem) {
            ctrlBuilder = createBuilder(clusterElem.childByPath("tuning"));
            nodeBuilder = createBuilder(clusterElem.childByPath("engine.proton"));
            if (nodeBuilder.getDiskLimit().isPresent() || nodeBuilder.getMemoryLimit().isPresent())
                deployLogger.logApplicationPackage(WARNING, "Setting proton resource limits in <engine><proton> " +
                        "should not be done directly. Set limits for cluster as described in " +
                        "https://docs.vespa.ai/en/reference/services-content.html#resource-limits instead.");

            deriveLimits();
            return new ClusterResourceLimits(this);
        }

        private ResourceLimits.Builder createBuilder(ModelElement element) {
            return element == null
                    ? new ResourceLimits.Builder()
                    : DomResourceLimitsBuilder.createBuilder(element, hostedVespa);
        }

        public void setClusterControllerBuilder(ResourceLimits.Builder builder) {
            ctrlBuilder = builder;
        }

        public void setContentNodeBuilder(ResourceLimits.Builder builder) {
            nodeBuilder = builder;
        }

        public ClusterResourceLimits build() {
            deriveLimits();
            return new ClusterResourceLimits(this);
        }

        private void deriveLimits() {
            // This also ensures that content nodes limits are derived according to the formula in calcContentNodeLimit().
            considerSettingDefaultClusterControllerLimit(ctrlBuilder.getDiskLimit(),
                                                         nodeBuilder.getDiskLimit(),
                                                         ctrlBuilder::setDiskLimit,
                                                         resourceLimitDisk);
            considerSettingDefaultClusterControllerLimit(ctrlBuilder.getMemoryLimit(),
                                                         nodeBuilder.getMemoryLimit(),
                                                         ctrlBuilder::setMemoryLimit,
                                                         resourceLimitMemory);
            considerSettingDefaultClusterControllerLimit(ctrlBuilder.getAddressSpaceLimit(),
                                                         nodeBuilder.getAddressSpaceLimit(),
                                                         ctrlBuilder::setAddressSpaceLimit,
                                                         resourceLimitLowAddressSpace);

            deriveClusterControllerLimit(ctrlBuilder.getDiskLimit(), nodeBuilder.getDiskLimit(), ctrlBuilder::setDiskLimit);
            deriveClusterControllerLimit(ctrlBuilder.getMemoryLimit(), nodeBuilder.getMemoryLimit(), ctrlBuilder::setMemoryLimit);
            deriveClusterControllerLimit(ctrlBuilder.getAddressSpaceLimit(), nodeBuilder.getAddressSpaceLimit(), ctrlBuilder::setAddressSpaceLimit);

            deriveContentNodeLimit(nodeBuilder.getDiskLimit(), ctrlBuilder.getDiskLimit(), 0.6, nodeBuilder::setDiskLimit);
            deriveContentNodeLimit(nodeBuilder.getMemoryLimit(), ctrlBuilder.getMemoryLimit(), 0.5, nodeBuilder::setMemoryLimit);
            deriveContentNodeLimit(nodeBuilder.getAddressSpaceLimit(), ctrlBuilder.getAddressSpaceLimit(), 0.5, nodeBuilder::setAddressSpaceLimit);

            ctrlBuilder.setLowWatermarkDifference(resourceLimitLowWatermarkDifference);
            nodeBuilder.setLowWatermarkDifference(resourceLimitLowWatermarkDifference);
        }

        private void considerSettingDefaultClusterControllerLimit(Optional<Double> clusterControllerLimit,
                                                                  Optional<Double> contentNodeLimit,
                                                                  Consumer<Double> setter,
                                                                  double resourceLimit) {
            // TODO: remove this when feed block in distributor is default enabled.
            if (clusterControllerLimit.isEmpty() && contentNodeLimit.isEmpty()) {
                setter.accept(resourceLimit);
            }
        }

        private void deriveClusterControllerLimit(Optional<Double> clusterControllerLimit,
                                                  Optional<Double> contentNodeLimit,
                                                  Consumer<Double> setter) {
            if (clusterControllerLimit.isEmpty()) {
                contentNodeLimit.ifPresent(limit ->
                        // TODO: emit warning when feed block in distributor is default enabled.
                        setter.accept(Double.max(0.0, (limit - 0.01))));
            }
        }

        private void deriveContentNodeLimit(Optional<Double> contentNodeLimit,
                                            Optional<Double> clusterControllerLimit,
                                            double scaleFactor,
                                            Consumer<Double> setter) {
            if (contentNodeLimit.isEmpty()) {
                clusterControllerLimit.ifPresent(limit ->
                        setter.accept(calcContentNodeLimit(limit, scaleFactor)));
            }
        }

        private double calcContentNodeLimit(double clusterControllerLimit, double scaleFactor) {
            return clusterControllerLimit + ((1.0 - clusterControllerLimit) * scaleFactor);
        }


        private void verifyLimits(double resourceLimitDisk, double resourceLimitMemory,
                                  double resourceLimitLowWatermarkDifference, double resourceLimitAddressSpace) {
            verifyLimitInRange(resourceLimitDisk, "disk");
            verifyLimitInRange(resourceLimitMemory, "memory");
            verifyLimitInRange(resourceLimitLowWatermarkDifference, "low watermark difference");
            verifyLimitInRange(resourceLimitAddressSpace, "address space");
        }

        private void verifyLimitInRange(double limit, String type) {
            String message = "Resource limit for " + type + " is set to illegal value " + limit +
                       ", but must be in the range [0.0, 1.0]";
            if (limit < 0.0)
                throw new IllegalArgumentException(message);

            if (limit > 1.0)
                throw new IllegalArgumentException(message);
        }

    }

}
