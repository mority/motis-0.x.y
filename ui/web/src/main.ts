import { createApp } from 'vue'
import App from './App.vue'
import router from './router'
import DateTimeService from './services/DateTimeService';
import MOTISPostService from './services/MOTISPostService';
import TranslationService from './services/TranslationService';

const app = createApp(App)
app.use(TranslationService, "de-DE");
const interval = setInterval(() => {
    if (TranslationService.service !== null && TranslationService.service.isLoaded) {
        app.use(router(TranslationService.service));
        app.use(MOTISPostService);
        const now = new Date();
        const initialDateTime = new Date(2021, 11, 25, now.getHours(), now.getMinutes(), now.getSeconds(), now.getMilliseconds()).valueOf()
        app.use(DateTimeService, TranslationService.service, initialDateTime);
        app.mount('#app');
        clearInterval(interval);
    }
}, 10);

