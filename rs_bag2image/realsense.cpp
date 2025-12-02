#include "realsense.h"
#include "version.h"

#include <sstream>
#include <iomanip>
#include <limits>
#include <fstream>

// Constructor
RealSense::RealSense( int argc, char* argv[] )
{
    std::cout << "rs_bag2image " << RS_BAG2IMAGE_VERSION << std::endl;

    // Initialize
    initialize( argc, argv );
}

// Destructor
RealSense::~RealSense()
{
    // Finalize
    finalize();
}

// Processing
void RealSense::run()
{
    // Retrieve Last Position
    uint64_t last_position = pipeline_profile.get_device().as<rs2::playback>().get_position();

    // Main Loop
    while( true ){
        // Update Data
        update();

        // Draw Data
        draw();

        // Show Data
        if( display ){
            show();
        }

        // Save Data
        save();

        // Increment frame count and show progress
        frame_count++;
        const uint64_t current_position = pipeline_profile.get_device().as<rs2::playback>().get_position();
        showProgress( current_position );

        // Key Check
        const int32_t key = cv::waitKey( 1 );
        if( key == 'q' ){
            break;
        }

        // End of Position
        if( static_cast<int64_t>( current_position - last_position ) < 0 ){
            std::cout << std::endl; // New line after progress bar
            break;
        }
        last_position = current_position;
    }
}

// Initialize
void RealSense::initialize( int argc, char * argv[] )
{
    cv::setUseOptimized( true );

    // Initialize Parameter
    initializeParameter( argc, argv );

    // Initialize Sensor
    initializeSensor();

    // Initialize Save
    initializeSave();
}

// Initialize Parameter
inline void RealSense::initializeParameter( int argc, char * argv[] )
{
    // Create Command Line Parser
    const std::string keys =
        "{ help h    |       | print this message.                                                      }"
        "{ bag b     |       | path to input bag file. (required)                                       }"
        "{ scaling s | false | enable depth scaling for visualization. false is raw 16bit image. (bool) }"
        "{ quality q | 95    | jpeg encoding quality for color and infrared. [0-100]                    }"
        "{ display d | false | display each stream images on window. false is not display. (bool)       }";
    cv::CommandLineParser parser( argc, argv, keys );

    if( parser.has( "help" ) ){
        parser.printMessage();
        std::exit( EXIT_SUCCESS );
    }

    // Check Parsing Error
    if( !parser.check() ){
        parser.printErrors();
        throw std::runtime_error( "failed command arguments" );
    }

    // Retrieve Bag File Path (Required)
    if( !parser.has( "bag" ) ){
        throw std::runtime_error( "failed can't find input bag file" );
    }
    else{
        bag_file = parser.get<cv::String>( "bag" ).c_str();
        if( !filesystem::is_regular_file( bag_file ) || bag_file.extension() != ".bag" ){
            throw std::runtime_error( "failed can't find input bag file" );
        }
    }

    // Retrieve Scaling Flag (Option)
    if( !parser.has( "scaling" ) ){
        scaling = false;
    }
    else{
        scaling = parser.get<bool>( "scaling" );
    }

    // Retrieve JPEG Quality (Option)
    if( !parser.has( "quality" ) ){
        params = { cv::IMWRITE_JPEG_QUALITY, 95 };
    }
    else{
        params = { cv::IMWRITE_JPEG_QUALITY, std::min( std::max( 0, parser.get<int32_t>( "quality" ) ), 100 ) };
    }

    // Retrieve Display Flag (Option)
    if( !parser.has( "display" ) ){
        display = false;
    }
    else{
        display = parser.get<bool>( "display" );
    }
}

// Initialize Sensor
inline void RealSense::initializeSensor()
{
    // Retrieve Each Streams that contain in File
    rs2::config config;
    rs2::context context;
    const rs2::playback playback = context.load_device( bag_file.string() );
    const std::vector<rs2::sensor> sensors = playback.query_sensors();
    for( const rs2::sensor& sensor : sensors ){
        const std::vector<rs2::stream_profile> stream_profiles = sensor.get_stream_profiles();
        for( const rs2::stream_profile& stream_profile : stream_profiles ){
            config.enable_stream( stream_profile.stream_type(), stream_profile.stream_index() );
        }
    }

    // Start Pipeline
    config.enable_device_from_file( playback.file_name() );
    pipeline_profile = pipeline.start( config );

    // Set Non Real Time Playback
    pipeline_profile.get_device().as<rs2::playback>().set_real_time( false );

    // Get Total Duration for Progress Bar
    total_duration = pipeline_profile.get_device().as<rs2::playback>().get_duration().count();
    frame_count = 0;

    // Show Enable Streams
    const std::vector<rs2::stream_profile> stream_profiles = pipeline_profile.get_streams();
    for( const rs2::stream_profile stream_profile : stream_profiles ){
        std::cout << stream_profile.stream_name() << std::endl;
    }
    std::cout << std::endl;
}

// Initialize Save
inline void RealSense::initializeSave()
{
    // Create Root Directory (Bag File Name)
    directory = bag_file.parent_path().generic_string() + "/" + bag_file.stem().string();
    if( !filesystem::create_directories( directory ) ){
        throw std::runtime_error( "failed can't create root directory" );
    }

    // Create Sub Directory for Each Streams (Stream Name)
    const std::vector<rs2::stream_profile> stream_profiles = pipeline_profile.get_streams();
    for( const rs2::stream_profile stream_profile : stream_profiles ){
        filesystem::path sub_directory = directory.generic_string() + "/" + stream_profile.stream_name();
        filesystem::create_directories( sub_directory );
    }
}

// Finalize
void RealSense::finalize()
{
    // Close Windows
    cv::destroyAllWindows();

    // Stop Pipline
    pipeline.stop();
}

// Update Data
void RealSense::update()
{
    // Update Frame
    updateFrame();

    // Update Color
    updateColor();

    // Update Depth
    updateDepth();

    // Update Infrared
    updateInfrared();

    // Update Gyro
    updateGyro();

    // Update Accel
    updateAccel();
}

// Update Frame
inline void RealSense::updateFrame()
{
    // Update Frame
    frameset = pipeline.wait_for_frames();
}

// Update Color
inline void RealSense::updateColor()
{
    // Retrieve Color Flame
    color_frame = frameset.get_color_frame();
    if( !color_frame ){
        return;
    }

    // Retrive Frame Size
    color_width = color_frame.as<rs2::video_frame>().get_width();
    color_height = color_frame.as<rs2::video_frame>().get_height();
}

// Update Depth
inline void RealSense::updateDepth()
{
    // Retrieve Depth Flame
    depth_frame = frameset.get_depth_frame();
    if( !depth_frame ){
        return;
    }

    // Retrive Frame Size
    depth_width = depth_frame.as<rs2::video_frame>().get_width();
    depth_height = depth_frame.as<rs2::video_frame>().get_height();
}

// Update Infrared
inline void RealSense::updateInfrared()
{
    // Retrieve Infrared Flames
    #if 29 < RS2_API_MINOR_VERSION
    frameset.foreach_rs( [this]( const rs2::frame& frame ){
    #else
    frameset.foreach( [this]( const rs2::frame& frame ){
    #endif
        if( frame.get_profile().stream_type() == rs2_stream::RS2_STREAM_INFRARED ){
            const int32_t infrared_stream_index = frame.get_profile().stream_index();
            const int32_t infrared_frame_index = ( infrared_stream_index != 0 ) ? infrared_stream_index - 1 : 0;
            infrared_frames[infrared_frame_index] = frame;
        }
    } );

    const rs2::frame& infrared_frame = infrared_frames.front();
    if( !infrared_frame ){
        return;
    }

    // Retrive Frame Size
    infrared_width = infrared_frame.as<rs2::video_frame>().get_width();
    infrared_height = infrared_frame.as<rs2::video_frame>().get_height();
}

// Update Gyro
inline void RealSense::updateGyro()
{
    // Retrieve Gyro Frame
    #if 29 < RS2_API_MINOR_VERSION
    frameset.foreach_rs( [this]( const rs2::frame& frame ){
    #else
    frameset.foreach( [this]( const rs2::frame& frame ){
    #endif
        if( frame.get_profile().stream_type() == rs2_stream::RS2_STREAM_GYRO ){
            gyro_frame = frame;
        }
    } );

    if( !gyro_frame ){
        return;
    }

    // Get Motion Data
    rs2::motion_frame motion = gyro_frame.as<rs2::motion_frame>();
    gyro_data = motion.get_motion_data();
    gyro_timestamp = gyro_frame.get_timestamp();
}

// Update Accel
inline void RealSense::updateAccel()
{
    // Retrieve Accel Frame
    #if 29 < RS2_API_MINOR_VERSION
    frameset.foreach_rs( [this]( const rs2::frame& frame ){
    #else
    frameset.foreach( [this]( const rs2::frame& frame ){
    #endif
        if( frame.get_profile().stream_type() == rs2_stream::RS2_STREAM_ACCEL ){
            accel_frame = frame;
        }
    } );

    if( !accel_frame ){
        return;
    }

    // Get Motion Data
    rs2::motion_frame motion = accel_frame.as<rs2::motion_frame>();
    accel_data = motion.get_motion_data();
    accel_timestamp = accel_frame.get_timestamp();
}

// Draw Data
void RealSense::draw()
{
    // Draw Color
    drawColor();

    // Draw Depth
    drawDepth();

    // Draw Infrared
    drawInfrared();
}

// Draw Color
inline void RealSense::drawColor()
{
    if( !color_frame ){
        return;
    }

    // Create cv::Mat form Color Frame
    const rs2_format color_format = color_frame.get_profile().format();
    switch( color_format ){
        // RGB8
        case rs2_format::RS2_FORMAT_RGB8:
        {
            color_mat = cv::Mat( color_height, color_width, CV_8UC3, const_cast<void*>( color_frame.get_data() ) ).clone();
            cv::cvtColor( color_mat, color_mat, cv::COLOR_RGB2BGR );
            break;
        }
        // RGBA8
        case rs2_format::RS2_FORMAT_RGBA8:
        {
            color_mat = cv::Mat( color_height, color_width, CV_8UC4, const_cast<void*>( color_frame.get_data() ) ).clone();
            cv::cvtColor( color_mat, color_mat, cv::COLOR_RGBA2BGRA );
            break;
        }
        // BGR8
        case rs2_format::RS2_FORMAT_BGR8:
        {
            color_mat = cv::Mat( color_height, color_width, CV_8UC3, const_cast<void*>( color_frame.get_data() ) ).clone();
            break;
        }
        // BGRA8
        case rs2_format::RS2_FORMAT_BGRA8:
        {
            color_mat = cv::Mat( color_height, color_width, CV_8UC4, const_cast<void*>( color_frame.get_data() ) ).clone();
            break;
        }
        // Y16 (GrayScale)
        case rs2_format::RS2_FORMAT_Y16:
        {
            color_mat = cv::Mat( color_height, color_width, CV_16UC1, const_cast<void*>( color_frame.get_data() ) ).clone();
            constexpr double scaling = static_cast<double>( std::numeric_limits<uint8_t>::max() ) / static_cast<double>( std::numeric_limits<uint16_t>::max() );
            color_mat.convertTo( color_mat, CV_8U, scaling );
            break;
        }
        // YUYV
        case rs2_format::RS2_FORMAT_YUYV:
        {
            color_mat = cv::Mat( color_height, color_width, CV_8UC2, const_cast<void*>( color_frame.get_data() ) ).clone();
            cv::cvtColor( color_mat, color_mat, cv::COLOR_YUV2BGR_YUYV );
            break;
        }
        default:
            throw std::runtime_error( "unknown color format" );
            break;
    }
}

// Draw Depth
inline void RealSense::drawDepth()
{
    if( !depth_frame ){
        return;
    }

    // Create cv::Mat form Depth Frame
    depth_mat = cv::Mat( depth_height, depth_width, CV_16UC1, const_cast<void*>( depth_frame.get_data() ) ).clone();
}

// Draw Infrared
inline void RealSense::drawInfrared()
{
    // Create cv::Mat form Infrared Frame
    for( const rs2::frame& infrared_frame : infrared_frames ){
        if( !infrared_frame ){
            continue;
        }

        const uint8_t infrared_stream_index = infrared_frame.get_profile().stream_index();
        const uint8_t infrared_mat_index = ( infrared_stream_index != 0 ) ? infrared_stream_index - 1 : 0;
        const rs2_format infrared_format = infrared_frame.get_profile().format();
        switch( infrared_format ){
            // RGB8 (Color)
            case rs2_format::RS2_FORMAT_RGB8:
            {
                infrared_mats[infrared_mat_index] = cv::Mat( infrared_height, infrared_width, CV_8UC3, const_cast<void*>( infrared_frame.get_data() ) ).clone();
                cv::cvtColor( infrared_mats[infrared_mat_index], infrared_mats[infrared_mat_index], cv::COLOR_RGB2BGR );
                break;
            }
            // RGBA8 (Color)
            case rs2_format::RS2_FORMAT_RGBA8:
            {
                infrared_mats[infrared_mat_index] = cv::Mat( infrared_height, infrared_width, CV_8UC4, const_cast<void*>( infrared_frame.get_data() ) ).clone();
                cv::cvtColor( infrared_mats[infrared_mat_index], infrared_mats[infrared_mat_index], cv::COLOR_RGBA2BGRA );
                break;
            }
            // BGR8 (Color)
            case rs2_format::RS2_FORMAT_BGR8:
            {
                infrared_mats[infrared_mat_index] = cv::Mat( infrared_height, infrared_width, CV_8UC3, const_cast<void*>( infrared_frame.get_data() ) ).clone();
                break;
            }
            // BGRA8 (Color)
            case rs2_format::RS2_FORMAT_BGRA8:
            {
                infrared_mats[infrared_mat_index] = cv::Mat( infrared_height, infrared_width, CV_8UC4, const_cast<void*>( infrared_frame.get_data() ) ).clone();
                break;
            }
            // Y8
            case rs2_format::RS2_FORMAT_Y8:
            {
                infrared_mats[infrared_mat_index] = cv::Mat( infrared_height, infrared_width, CV_8UC1, const_cast<void*>( infrared_frame.get_data() ) ).clone();
                break;
            }
            // UYVY
            case rs2_format::RS2_FORMAT_UYVY:
            {
                infrared_mats[infrared_mat_index] = cv::Mat( infrared_height, infrared_width, CV_8UC2, const_cast<void*>( infrared_frame.get_data() ) ).clone();
                cv::cvtColor( infrared_mats[infrared_mat_index], infrared_mats[infrared_mat_index], cv::COLOR_YUV2GRAY_UYVY );
                break;
            }
            default:
                throw std::runtime_error( "unknown infrared format" );
                break;
        }
    }
}

// Show Data
void RealSense::show()
{
    // Show Color
    showColor();

    // Show Depth
    showDepth();

    // Show Infrared
    showInfrared();
}

// Show Color
inline void RealSense::showColor()
{
    if( !color_frame ){
        return;
    }

    if( color_mat.empty() ){
        return;
    }

    // Show Color Image
    cv::imshow( "Color", color_mat );
}

// Show Depth
inline void RealSense::showDepth()
{
    if( !depth_frame ){
        return;
    }

    if( depth_mat.empty() ){
        return;
    }

    // Scaling
    cv::Mat scale_mat;
    depth_mat.convertTo( scale_mat, CV_8U, -255.0 / 10000.0, 255.0 ); // 0-10000 -> 255(white)-0(black)

    // Show Depth Image
    cv::imshow( "Depth", scale_mat );
}

// Show Infrared
inline void RealSense::showInfrared()
{
    for( const rs2::frame& infrared_frame : infrared_frames ){
        if( !infrared_frame ){
            continue;
        }

        const uint8_t infrared_stream_index = infrared_frame.get_profile().stream_index();
        const uint8_t infrared_mat_index = ( infrared_stream_index != 0 ) ? infrared_stream_index - 1 : 0;
        if( infrared_mats[infrared_mat_index].empty() ){
            continue;
        }

        // Show Infrared Image
        cv::imshow( "Infrared " + std::to_string( infrared_stream_index ), infrared_mats[infrared_mat_index] );
    }
}

// Save Data
void RealSense::save()
{
    // Save Color
    saveColor();

    // Save Depth
    saveDepth();

    // Save Infrared
    saveInfrared();

    // Save Gyro
    saveGyro();

    // Save Accel
    saveAccel();
}

// Save Color
inline void RealSense::saveColor()
{
    if( !color_frame ){
        return;
    }

    if( color_mat.empty() ){
        return;
    }

    // Create Save Directory and File Name
    std::ostringstream oss;
    oss << directory.generic_string() << "/Color/";
    oss << std::setfill( '0' ) << std::setw( 6 ) << color_frame.get_frame_number() << ".jpg";

    // Write Color Image
    cv::imwrite( oss.str(), color_mat, params );

    // Save Metadata
    std::ostringstream meta_oss;
    meta_oss << directory.generic_string() << "/Color/metadata.csv";
    filesystem::path meta_path = meta_oss.str();
    bool write_header = !filesystem::exists( meta_path );
    std::ofstream meta_file( meta_oss.str(), std::ios::app );
    if( meta_file.is_open() ){
        if( write_header ){
            meta_file << "frame_number,timestamp,width,height,format" << std::endl;
        }
        meta_file << std::fixed << std::setprecision( 6 );
        meta_file << color_frame.get_frame_number() << ",";
        meta_file << color_frame.get_timestamp() << ",";
        meta_file << color_width << ",";
        meta_file << color_height << ",";
        meta_file << color_frame.get_profile().format() << std::endl;
        meta_file.close();
    }
}

// Save Depth
inline void RealSense::saveDepth()
{
    if( !depth_frame ){
        return;
    }

    if( depth_mat.empty() ){
        return;
    }

    // Create Save Directory and File Name
    std::ostringstream oss;
    oss << directory.generic_string() << "/Depth/";
    oss << std::setfill( '0' ) << std::setw( 6 ) << depth_frame.get_frame_number() << ".png";

    // Scaling
    cv::Mat scale_mat = depth_mat;
    if( scaling ){
        depth_mat.convertTo( scale_mat, CV_8U, -255.0 / 10000.0, 255.0 ); // 0-10000 -> 255(white)-0(black)
    }

    // Write Depth Image
    cv::imwrite( oss.str(), scale_mat );

    // Save Metadata
    std::ostringstream meta_oss;
    meta_oss << directory.generic_string() << "/Depth/metadata.csv";
    filesystem::path meta_path = meta_oss.str();
    bool write_header = !filesystem::exists( meta_path );
    std::ofstream meta_file( meta_oss.str(), std::ios::app );
    if( meta_file.is_open() ){
        if( write_header ){
            meta_file << "frame_number,timestamp,width,height,format" << std::endl;
        }
        meta_file << std::fixed << std::setprecision( 6 );
        meta_file << depth_frame.get_frame_number() << ",";
        meta_file << depth_frame.get_timestamp() << ",";
        meta_file << depth_width << ",";
        meta_file << depth_height << ",";
        meta_file << depth_frame.get_profile().format() << std::endl;
        meta_file.close();
    }
}

// Save Infrared
inline void RealSense::saveInfrared()
{
    for( const rs2::frame& infrared_frame : infrared_frames ){
        if( !infrared_frame ){
            continue;
        }

        const uint8_t infrared_stream_index = infrared_frame.get_profile().stream_index();
        const uint8_t infrared_mat_index = ( infrared_stream_index != 0 ) ? infrared_stream_index - 1 : 0;
        if( infrared_mats[infrared_mat_index].empty() ){
            continue;
        }

        // Create Save Directory and File Name
        std::ostringstream oss;
        // Use cleaner directory names: IR for left (index 1), IR_Right for right (index 2)
        if( infrared_stream_index == 2 ){
            oss << directory.generic_string() << "/IR_Right/";
        }
        else{
            oss << directory.generic_string() << "/IR/";
        }
        oss << std::setfill( '0' ) << std::setw( 6 ) << infrared_frame.get_frame_number() << ".jpg";

        // Write Infrared Image
        cv::imwrite( oss.str(), infrared_mats[infrared_mat_index], params );

        // Save Metadata
        std::ostringstream meta_oss;
        if( infrared_stream_index == 2 ){
            meta_oss << directory.generic_string() << "/IR_Right/metadata.csv";
        }
        else{
            meta_oss << directory.generic_string() << "/IR/metadata.csv";
        }
        filesystem::path meta_path = meta_oss.str();
        bool write_header = !filesystem::exists( meta_path );
        std::ofstream meta_file( meta_oss.str(), std::ios::app );
        if( meta_file.is_open() ){
            if( write_header ){
                meta_file << "frame_number,timestamp,width,height,format" << std::endl;
            }
            meta_file << std::fixed << std::setprecision( 6 );
            meta_file << infrared_frame.get_frame_number() << ",";
            meta_file << infrared_frame.get_timestamp() << ",";
            meta_file << infrared_width << ",";
            meta_file << infrared_height << ",";
            meta_file << infrared_frame.get_profile().format() << std::endl;
            meta_file.close();
        }
    }
}

// Save Gyro
inline void RealSense::saveGyro()
{
    if( !gyro_frame ){
        return;
    }

    // Create IMU Directory if it doesn't exist
    filesystem::path imu_dir = directory.generic_string() + "/IMU";
    if( !filesystem::exists( imu_dir ) ){
        filesystem::create_directories( imu_dir );
    }

    // Create Save Directory and File Name
    std::ostringstream oss;
    oss << directory.generic_string() << "/IMU/gyro_data.csv";

    // Open File (append mode)
    std::ofstream file;
    filesystem::path file_path = oss.str();

    // Check if file exists to write header
    bool write_header = !filesystem::exists( file_path );
    file.open( oss.str(), std::ios::app );

    if( !file.is_open() ){
        return;
    }

    // Write Header
    if( write_header ){
        file << "frame_number,timestamp,x,y,z" << std::endl;
    }

    // Write Gyro Data
    file << std::fixed << std::setprecision( 6 );
    file << gyro_frame.get_frame_number() << ",";
    file << gyro_timestamp << ",";
    file << gyro_data.x << ",";
    file << gyro_data.y << ",";
    file << gyro_data.z << std::endl;

    file.close();
}

// Save Accel
inline void RealSense::saveAccel()
{
    if( !accel_frame ){
        return;
    }

    // Create IMU Directory if it doesn't exist
    filesystem::path imu_dir = directory.generic_string() + "/IMU";
    if( !filesystem::exists( imu_dir ) ){
        filesystem::create_directories( imu_dir );
    }

    // Create Save Directory and File Name
    std::ostringstream oss;
    oss << directory.generic_string() << "/IMU/accel_data.csv";

    // Open File (append mode)
    std::ofstream file;
    filesystem::path file_path = oss.str();

    // Check if file exists to write header
    bool write_header = !filesystem::exists( file_path );
    file.open( oss.str(), std::ios::app );

    if( !file.is_open() ){
        return;
    }

    // Write Header
    if( write_header ){
        file << "frame_number,timestamp,x,y,z" << std::endl;
    }

    // Write Accel Data
    file << std::fixed << std::setprecision( 6 );
    file << accel_frame.get_frame_number() << ",";
    file << accel_timestamp << ",";
    file << accel_data.x << ",";
    file << accel_data.y << ",";
    file << accel_data.z << std::endl;

    file.close();
}

// Show Progress Bar
inline void RealSense::showProgress( uint64_t current_position )
{
    if( total_duration == 0 ){
        return;
    }

    // Calculate percentage
    double percentage = ( static_cast<double>( current_position ) / static_cast<double>( total_duration ) ) * 100.0;
    percentage = std::min( percentage, 100.0 );

    // Create progress bar
    const int bar_width = 50;
    int filled_width = static_cast<int>( bar_width * percentage / 100.0 );

    std::cout << "\r"; // Return to beginning of line
    std::cout << "Progress: [";

    for( int i = 0; i < bar_width; ++i ){
        if( i < filled_width ){
            std::cout << "=";
        }
        else if( i == filled_width ){
            std::cout << ">";
        }
        else{
            std::cout << " ";
        }
    }

    std::cout << "] " << std::fixed << std::setprecision( 1 ) << percentage << "% ";
    std::cout << "(" << frame_count << " frames)";
    std::cout << std::flush;
}
