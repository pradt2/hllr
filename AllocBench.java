package com.sage.ordermanagement;

import java.time.Duration;
import java.time.Instant;

public class AllocBench {

    public static class Node {
        int value;
    }

    private static Node recursiveFunction(int recursionLevel) {

        Node node = new Node();
        node.value = recursionLevel;

        Node node1;

        if (recursionLevel > 0) {
            node1 = recursiveFunction(recursionLevel - 1);
        } else return node;

        return node.hashCode() < node1.hashCode() ? node : node1;
    }

    public static void main(String... args) {
        Instant instant = Instant.now();

        for (int i = 0; i < 1 << 22; i++) {
            recursiveFunction(100);
        }

        Instant now = Instant.now();
        System.out.println(Duration.between(instant, now).toMillis());
    }
}
