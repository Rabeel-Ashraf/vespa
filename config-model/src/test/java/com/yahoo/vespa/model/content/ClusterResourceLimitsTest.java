// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.vespa.model.content;

import com.yahoo.config.application.api.DeployLogger;
import com.yahoo.config.model.api.ModelContext;
import com.yahoo.config.model.deploy.TestProperties;
import com.yahoo.text.XML;
import com.yahoo.vespa.model.builder.xml.dom.ModelElement;
import org.junit.jupiter.api.Test;
import org.w3c.dom.Document;

import java.util.Optional;
import java.util.logging.Level;

import static org.junit.jupiter.api.Assertions.*;

/**
 * @author Geir Storli
 */
public class ClusterResourceLimitsTest {

    private static class Fixture {
        private final boolean hostedVespa;
        private final ResourceLimits.Builder ctrlBuilder = new ResourceLimits.Builder();
        private final ResourceLimits.Builder nodeBuilder = new ResourceLimits.Builder();
        private double lowWatermarkDifference = 0.0;

        public Fixture() {
            this(false);
        }

        public Fixture(boolean hostedVespa) {
            this.hostedVespa = hostedVespa;
        }

        public Fixture ctrlDisk(double limit) {
            ctrlBuilder.setDiskLimit(limit);
            return this;
        }
        public Fixture ctrlMemory(double limit) {
            ctrlBuilder.setMemoryLimit(limit);
            return this;
        }
        public Fixture ctrlAddressSpace(double limit) {
            ctrlBuilder.setAddressSpaceLimit(limit);
            return this;
        }
        public Fixture nodeDisk(double limit) {
            nodeBuilder.setDiskLimit(limit);
            return this;
        }
        public Fixture nodeMemory(double limit) {
            nodeBuilder.setMemoryLimit(limit);
            return this;
        }
        public Fixture nodeAddressSpace(double limit) {
            nodeBuilder.setAddressSpaceLimit(limit);
            return this;
        }
        public Fixture lowWatermarkDifference(double lowWatermarkDifference) {
            this.lowWatermarkDifference = lowWatermarkDifference;
            return this;
        }
        public ClusterResourceLimits build() {
            ModelContext.FeatureFlags featureFlags = new TestProperties();
            var builder = new ClusterResourceLimits.Builder(hostedVespa,
                                                            featureFlags.resourceLimitDisk(),
                                                            featureFlags.resourceLimitMemory(),
                                                            lowWatermarkDifference,
                                                            featureFlags.resourceLimitAddressSpace(),
                                                            new TestLogger());
            builder.setClusterControllerBuilder(ctrlBuilder);
            builder.setContentNodeBuilder(nodeBuilder);
            return builder.build();
        }
    }

    @Test
    void content_node_limits_are_derived_from_cluster_controller_limits_if_not_set() {
        assertLimits(0.4, 0.7, 0.88, 0.76, 0.85, 0.94, 0.05,
                new Fixture().ctrlDisk(0.4).ctrlMemory(0.7).lowWatermarkDifference(0.05).ctrlAddressSpace(0.88));
        assertLimits(0.75, 0.8, 0.89, 0.9, 0.9, 0.945, 0.0,
                new Fixture());
    }

    @Test
    void content_node_limits_can_be_set_explicit() {
        assertLimits(0.4, 0.7, 0.88, 0.9, 0.95, 0.93, 0.0,
                new Fixture().ctrlDisk(0.4).ctrlMemory(0.7).ctrlAddressSpace(0.88).nodeDisk(0.9).nodeMemory(0.95).nodeAddressSpace(0.93));
        assertLimits(0.4, 0.8, 0.89, 0.95, 0.9, 0.945, 0.0,
                new Fixture().ctrlDisk(0.4).nodeDisk(0.95));
        assertLimits(0.75, 0.7, 0.89, 0.9, 0.95, 0.945, 0.0,
                new Fixture().ctrlMemory(0.7).nodeMemory(0.95));
    }

    @Test
    void cluster_controller_limits_are_equal_to_content_node_limits_minus_one_percent_if_not_set() {
        assertLimits(0.89, 0.94, 0.89, 0.9, 0.95, 0.945, 0.0,
                new Fixture().nodeDisk(0.9).nodeMemory(0.95));
        assertLimits(0.89, 0.8, 0.89, 0.9, 0.9, 0.945, 0.0,
                new Fixture().nodeDisk(0.9));
        assertLimits(0.75, 0.94, 0.89, 0.9, 0.95, 0.945, 0.0,
                new Fixture().nodeMemory(0.95));
        assertLimits(0.75, 0.0, 0.89, 0.9, 0.005, 0.945, 0.0,
                new Fixture().nodeMemory(0.005));
    }

    @Test
    void limits_are_derived_from_the_other_if_not_set() {
        assertLimits(0.6, 0.94, 0.89, 0.84, 0.95, 0.945, 0.0,
                new Fixture().ctrlDisk(0.6).nodeMemory(0.95));
        assertLimits(0.89, 0.7, 0.89, 0.9, 0.85, 0.945, 0.0,
                new Fixture().ctrlMemory(0.7).nodeDisk(0.9));
    }

    @Test
    void default_resource_limits_when_feed_block_is_enabled_in_distributor() {
        assertLimits(0.75, 0.8, 0.89, 0.9, 0.9, 0.945, 0.0,
                new Fixture(true));
    }

    @Test
    void hosted_exception_is_thrown_when_resource_limits_are_specified() {
        try {
            hostedBuild();
            fail();
        } catch (IllegalArgumentException e) {
            assertTrue(e.getMessage().contains("Element 'resource-limits' is not allowed to be set"));
        }
    }

    @Test
    void hosted_limits_from_feature_flag_are_used() {
        TestProperties featureFlags = new TestProperties();
        featureFlags.setResourceLimitDisk(0.85);
        featureFlags.setResourceLimitMemory(0.90);
        featureFlags.setResourceLimitAddressSpace(0.89);
        var limits = hostedBuild(featureFlags, false);

        // Verify that limits from feature flags are used
        assertLimits(0.85, 0.90, 0.0, 0.89, limits.getClusterControllerLimits());
        assertLimits(0.94, 0.95, 0.0, 0.945, limits.getContentNodeLimits());
    }

    @Test
    void exception_is_thrown_when_resource_limits_are_out_of_range() {
        TestProperties featureFlags = new TestProperties();
        featureFlags.setResourceLimitDisk(1.1);

        try {
            hostedBuild(featureFlags, false);
            fail();
        } catch (IllegalArgumentException e) {
            assertTrue(e.getMessage().contains("Resource limit for disk is set to illegal value 1.1, but must be in the range [0.0, 1.0]"));
        }

        featureFlags = new TestProperties();
        featureFlags.setResourceLimitDisk(-0.1);
        try {
            hostedBuild(featureFlags, false);
            fail();
        } catch (IllegalArgumentException e) {
            assertTrue(e.getMessage().contains("Resource limit for disk is set to illegal value -0.1, but must be in the range [0.0, 1.0]"));
        }
    }

    private ClusterResourceLimits hostedBuild() {
        return hostedBuild(new TestProperties(), true);
    }

    private ClusterResourceLimits hostedBuild(ModelContext.FeatureFlags featureFlags,
                                              boolean limitsInXml) {
        Document clusterXml = XML.getDocument("<cluster id=\"test\">" +
                                              "  <tuning>\n" +
                                              "    <resource-limits>\n" +
                                              "      <memory>0.92</memory>\n" +
                                              "    </resource-limits>\n" +
                                              "  </tuning>\n" +
                                              "</cluster>");

        Document noLimitsXml = XML.getDocument("<cluster id=\"test\">" +
                                              "</cluster>");

        ClusterResourceLimits.Builder builder = new ClusterResourceLimits.Builder(true,
                                                                                  featureFlags.resourceLimitDisk(),
                                                                                  featureFlags.resourceLimitMemory(),
                                                                                  0.0,
                                                                                  featureFlags.resourceLimitAddressSpace(),
                                                                                  new TestLogger());
        return builder.build(new ModelElement((limitsInXml ? clusterXml : noLimitsXml).getDocumentElement()));
    }

    private void assertLimits(Double expCtrlDisk, Double expCtrlMemory, Double expCtrlAddressSpace,
                              Double expNodeDisk, Double expNodeMemory, Double expNodeAddressSpace,
                              Double expLowWatermarkDifference, Fixture f) {
        var limits = f.build();
        assertLimits(expCtrlDisk, expCtrlMemory, expLowWatermarkDifference, expCtrlAddressSpace, limits.getClusterControllerLimits());
        assertLimits(expNodeDisk, expNodeMemory, expLowWatermarkDifference, expNodeAddressSpace, limits.getContentNodeLimits());
    }

    private void assertLimits(Double expDisk, Double expMemory, Double expLowWatermarkDifference,
                              Double expAddressSpace, ResourceLimits limits) {
        assertLimit(expDisk, limits.getDiskLimit(), "disk");
        assertLimit(expMemory, limits.getMemoryLimit(), "memory");
        assertLimit(expLowWatermarkDifference, limits.getLowWatermarkDifference(), "lowWaterMarkDifference");
        assertLimit(expAddressSpace, limits.getAddressSpaceLimit(), "address space");
    }

    private void assertLimit(Double expLimit, Optional<Double> actLimit, String limitType) {
        if (expLimit == null) {
            assertFalse(actLimit.isPresent());
        } else {
            assertEquals(expLimit, actLimit.get(), 0.00001, limitType + " limit not as expected");
        }
    }

    private static class TestLogger implements DeployLogger {

        @Override
        public void log(Level level, String message) { /* Do nothing */ }

    }

}
