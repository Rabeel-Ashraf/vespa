// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.jdisc.http.server.jetty;

import com.yahoo.jdisc.http.HttpRequest;
import org.eclipse.jetty.http2.server.internal.HTTP2ServerConnection;
import org.eclipse.jetty.io.Connection;
import org.eclipse.jetty.server.Request;
import org.eclipse.jetty.server.internal.HttpConnection;

import java.util.Set;
import java.util.stream.Collectors;
import java.util.stream.Stream;

/**
 * @author bjorncs
 */
public class RequestUtils {
    public static final String JDISC_REQUEST_X509CERT = "jdisc.request.X509Certificate";
    public static final String JDISC_REQUEST_SSLSESSION = "jdisc.request.SSLSession";
    public static final String JDISC_REQUEST_CHAIN = "jdisc.request.chain";
    public static final String JDISC_RESPONSE_CHAIN = "jdisc.response.chain";

    // The local port as reported by servlet spec. This will be influenced by Host header and similar mechanisms.
    // The request URI uses the local listen port as the URI is used for handler routing/bindings.
    // Use this attribute for generating URIs that is presented to client.
    public static final String JDICS_REQUEST_PORT = "jdisc.request.port";

    static final Set<String> SUPPORTED_METHODS =
            Stream.of(HttpRequest.Method.OPTIONS, HttpRequest.Method.GET, HttpRequest.Method.HEAD,
                            HttpRequest.Method.POST, HttpRequest.Method.PUT, HttpRequest.Method.DELETE,
                            HttpRequest.Method.TRACE, HttpRequest.Method.PATCH)
                    .map(HttpRequest.Method::name)
                    .collect(Collectors.toSet());


    private RequestUtils() {}

    public static Connection getConnection(Request request) {
        return request.getConnectionMetaData().getConnection();
    }

    public static JDiscServerConnector getConnector(Request request) {
        return (JDiscServerConnector) request.getConnectionMetaData().getConnector();
    }

    static boolean isHttpServerConnection(Connection connection) {
        return connection instanceof HttpConnection || connection instanceof HTTP2ServerConnection;
    }

    /**
     * Note: HttpServletRequest.getLocalPort() may return the local port of the load balancer / reverse proxy if proxy-protocol is enabled.
     * @return the actual local port of the underlying Jetty connector
     */
    public static int getConnectorLocalPort(Request request) {
        JDiscServerConnector connector = getConnector(request);
        int actualLocalPort = connector.getLocalPort();
        int localPortIfConnectorUnopened = -1;
        int localPortIfConnectorClosed = -2;
        if (actualLocalPort == localPortIfConnectorUnopened || actualLocalPort == localPortIfConnectorClosed) {
            int configuredLocalPort = connector.listenPort();
            int localPortEphemeralPort = 0;
            if (configuredLocalPort == localPortEphemeralPort) {
                throw new IllegalStateException("Unable to determine connector's listen port");
            }
            return configuredLocalPort;
        }
        return actualLocalPort;
    }

}
