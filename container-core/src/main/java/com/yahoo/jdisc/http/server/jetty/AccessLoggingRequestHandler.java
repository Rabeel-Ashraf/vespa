// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.jdisc.http.server.jetty;

import com.yahoo.container.logging.AccessLogEntry;
import com.yahoo.jdisc.Request;
import com.yahoo.jdisc.handler.AbstractRequestHandler;
import com.yahoo.jdisc.handler.CompletionHandler;
import com.yahoo.jdisc.handler.ContentChannel;
import com.yahoo.jdisc.handler.DelegatedRequestHandler;
import com.yahoo.jdisc.handler.RequestHandler;
import com.yahoo.jdisc.handler.ResponseHandler;
import com.yahoo.jdisc.http.HttpHeaders;
import com.yahoo.jdisc.http.HttpRequest;

import java.io.ByteArrayOutputStream;
import java.nio.ByteBuffer;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Optional;
import java.util.concurrent.atomic.AtomicLong;

/**
 * A wrapper RequestHandler that enables access logging. By wrapping the request handler, we are able to wrap the
 * response handler as well. Hence, we can populate the access log entry with information from both the request
 * and the response. This wrapper also adds the access log entry to the request context, so that request handlers
 * may add information to it.
 *
 * Does not otherwise interfere with the request processing of the delegate request handler.
 *
 * @author bakksjo
 * @author bjorncs
 */
public class AccessLoggingRequestHandler extends AbstractRequestHandler implements DelegatedRequestHandler {
    public static final String CONTEXT_KEY_ACCESS_LOG_ENTRY
            = AccessLoggingRequestHandler.class.getName() + "_access-log-entry";

    public static Optional<AccessLogEntry> getAccessLogEntry(final HttpRequest jdiscRequest) {
        final Map<String, Object> requestContextMap = jdiscRequest.context();
        return getAccessLogEntry(requestContextMap);
    }

    public static Optional<AccessLogEntry> getAccessLogEntry(final Map<String, Object> requestContextMap) {
        return Optional.ofNullable(
                (AccessLogEntry) requestContextMap.get(CONTEXT_KEY_ACCESS_LOG_ENTRY));
    }

    private final org.eclipse.jetty.server.Request jettyRequest;
    private final RequestHandler delegateRequestHandler;
    private final AccessLogEntry accessLogEntry;

    public AccessLoggingRequestHandler(
            org.eclipse.jetty.server.Request jettyRequest,
            RequestHandler delegateRequestHandler,
            AccessLogEntry accessLogEntry) {
        this.jettyRequest = jettyRequest;
        this.delegateRequestHandler = delegateRequestHandler;
        this.accessLogEntry = accessLogEntry;

    }

    @Override
    public ContentChannel handleRequest(final Request request, final ResponseHandler handler) {
        final HttpRequest httpRequest = (HttpRequest) request;
        httpRequest.context().put(CONTEXT_KEY_ACCESS_LOG_ENTRY, accessLogEntry);
        var methodsWithEntity = List.of(HttpRequest.Method.POST, HttpRequest.Method.PUT, HttpRequest.Method.PATCH);
        var originalContentChannel = delegateRequestHandler.handleRequest(request, handler);
        var uriPath = request.getUri().getPath();
        if (methodsWithEntity.contains(httpRequest.getMethod())) {
            // Determine if the request content should be logged based on path prefix and effective sample rate
            long contentMaxSize = RequestUtils.getConnector(jettyRequest).requestContentLogging().stream()
                    .filter(rcl -> uriPath.startsWith(rcl.pathPrefix()) && rcl.samplingRate().shouldSample())
                    .map(rcl -> rcl.maxSize())
                    .findAny()
                    .orElse(0L);
            if (contentMaxSize > 0) return new ContentLoggingContentChannel(originalContentChannel, contentMaxSize);
        }
        return originalContentChannel;
    }

    @Override
    public RequestHandler getDelegate() {
        return delegateRequestHandler;
    }

    private class ContentLoggingContentChannel implements ContentChannel {
        final AtomicLong length = new AtomicLong();
        final ByteArrayOutputStream accumulatedRequestContent;
        final ContentChannel originalContentChannel;
        final long contentLoggingMaxSize;

        public ContentLoggingContentChannel(ContentChannel originalContentChannel, long contentLoggingMaxSize) {
            this.originalContentChannel = originalContentChannel;
            this.contentLoggingMaxSize = contentLoggingMaxSize;
            var contentLength = jettyRequest.getLength();
            this.accumulatedRequestContent =
                    new ByteArrayOutputStream(contentLength == -1 ? 128 : Math.toIntExact(contentLength));
        }

        @Override
        public void write(ByteBuffer buf, CompletionHandler handler) {
            length.addAndGet(buf.remaining());
            var bytesToLog = Math.toIntExact(Math.min(buf.remaining(), contentLoggingMaxSize - accumulatedRequestContent.size()));
            if (bytesToLog > 0) {
                if (buf.hasArray()) {
                    accumulatedRequestContent.write(buf.array(), buf.arrayOffset() + buf.position(), bytesToLog);
                } else {
                    byte[] temp = new byte[bytesToLog];
                    buf.get(temp);
                    accumulatedRequestContent.write(temp, 0, bytesToLog);
                    buf.position(buf.position() - bytesToLog); // Reset position to original
                }
            }
            if (originalContentChannel != null) originalContentChannel.write(buf, handler);
        }

        @Override
        public void close(CompletionHandler handler) {
            var bytes = accumulatedRequestContent.toByteArray();
            accessLogEntry.setContent(new AccessLogEntry.Content(
                    Objects.requireNonNullElse(jettyRequest.getHeaders().get(HttpHeaders.Names.CONTENT_TYPE), ""),
                    length.get(),
                    bytes));
            accumulatedRequestContent.reset();
            length.set(0);
            if (originalContentChannel != null) originalContentChannel.close(handler);
        }

        @Override
        public void onError(Throwable error) {
            if (originalContentChannel != null) originalContentChannel.onError(error);
        }
    }
}
