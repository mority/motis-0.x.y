import React from 'react';

import { Translations } from './Localization';
import { DatePicker } from './DatePicker';
import moment from 'moment';

export const SubOverlay: React.FC<{'translation': Translations, 'subOverlayHidden' : Boolean, 'setSubOverlayHidden': React.Dispatch<React.SetStateAction<Boolean>>}> = (props) => {
    
    // Current Date
    const [searchDate, setSearchDate] = React.useState<moment.Moment>(moment());
    
    return (
        <div className={props.subOverlayHidden ? 'sub-overlay hidden' : 'sub-overlay'}>
            <div id='sub-overlay-content'>
                <div className='trip-search'>
                    <div className='header'>
                        <div id='trip-search-form'>
                            <div className='pure-g gutters'>
                                <div className='pure-u-1 pure-u-sm-1-2 train-nr'>
                                    <div>
                                        <div className='label'>{props.translation.search.trainNr}</div>
                                        <div className='gb-input-group'>
                                            <div className='gb-input-icon'>
                                                <i className='icon'>train</i>
                                            </div>
                                            <input
                                                className='gb-input' tabIndex={1} type='number'
                                                pattern='[0-9]+' id='trip-search-trainnr-input' />
                                        </div>
                                    </div>
                                </div>
                            </div>
                            <div className='pure-g gutters'>
                                <div className='pure-u-1 pure-u-sm-12-24 to-location'>
                                <DatePicker translation={props.translation}
                                            currentDate={searchDate}
                                            setCurrentDate={setSearchDate}/>
                                </div>
                                <div className='pure-u-1 pure-u-sm-12-24'>
                                    <div>
                                        <div className='label'>{props.translation.search.time}</div>
                                        <div className='gb-input-group'>
                                            <div className='gb-input-icon'>
                                                <i className='icon'>schedule</i>
                                            </div>
                                            <input className='gb-input' tabIndex={4} />
                                            <div className='gb-input-widget'>
                                                <div className='hour-buttons'>
                                                    <div><a
                                                            className='gb-button gb-button-small gb-button-circle gb-button-outline gb-button-PRIMARY_COLOR disable-select'><i
                                                                className='icon'>chevron_left</i></a></div>
                                                    <div><a
                                                            className='gb-button gb-button-small gb-button-circle gb-button-outline gb-button-PRIMARY_COLOR disable-select'><i
                                                                className='icon'>chevron_right</i></a></div>
                                                </div>
                                            </div>
                                        </div>
                                    </div>
                                </div>
                            </div>
                        </div>
                    </div>
                    <div className='trips'></div>
                </div>
            </div>
            <div className='sub-overlay-close' onClick={() => props.setSubOverlayHidden(true)}><i className='icon'>close</i></div>
        </div>
    )
}