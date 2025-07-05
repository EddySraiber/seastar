# KV Server - Technical Debt & Future Improvements

This document outlines the current technical debt and planned improvements for the key-value server.

## Left Undone

### Performance & Testing
- **Performance Testing**: The service requires comprehensive performance testing and potential optimizations
- **Documentation**: ✅ Basic README completed

## Future Improvements

### Security & Communication
- **Authentication & Security**: Add API key authentication and basic security measures
- **Alternative Protocols**: Support additional communication methods beyond REST (WebSocket, gRPC)

### Performance Enhancements
- **Bulk Operations**: Support batch operations - put/delete many items for better performance
- **Dynamic Sharding**: Implement adaptive sharding formula based on usage patterns to distribute data more uniformly

### Additional Features
- **Monitoring**: Basic metrics and health monitoring
- **Error Handling**: Improved error responses and logging
- **Configuration**: Runtime configuration updates
